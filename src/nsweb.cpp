#include "nsweb.hpp"
#include "socket_fun.hpp"
#include "tls_fun.hpp"
#include "url_fun.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <algorithm>

namespace nsweb {

// Constructor
websocket_client::websocket_client()
    : curl_(nullptr), wslay_ctx_(nullptr), wslay_frame_ctx_(nullptr), is_connected_(false) {}

// Destructor
websocket_client::~websocket_client() {
    disconnect(true);
}

// Public methods
bool websocket_client::connect(const std::string& url) {
    if (is_connected()) {
        return false;
    }

    if(init_mbedtls() == false){
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to init mbedtls." << std::endl;
#endif
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_ = curl_easy_init();
    
    if (!perform_handshake(url)) {
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to perform handshake." << std::endl;
#endif
        curl_easy_cleanup(curl_);
        //curl_global_cleanup();
        return false;
    }
    setup_wslay_event_callbacks();
    is_connected_.store(true, std::memory_order_release);
    ws_thread_send = std::thread(&websocket_client::websocket_client_thread_send, this);
    ws_thread_recv = std::thread(&websocket_client::websocket_client_thread_recv, this);
    return true;
}

void websocket_client::disconnect(bool clear_curl) {
    if (!is_connected()) {
        if (clear_curl) {
            curl_global_cleanup();
        }
        return;
    }

    is_connected_.store(false, std::memory_order_release);
    curl_easy_cleanup(curl_);

    if (ws_thread_recv.joinable()) {
        ws_thread_recv.join();
    }
    if (ws_thread_send.joinable()) {
        ws_thread_send.join();
    }
    if (clear_curl) {
        curl_global_cleanup();
    }

    wslay_event_context_free(wslay_ctx_);
    wslay_frame_context_free(wslay_frame_ctx_);

    deinit_mbedtls();
}

bool websocket_client::is_connected() const {
    return is_connected_.load(std::memory_order_acquire);
}

bool websocket_client::send(const std::string &data) {
    if (!is_connected()) {
        return false;
    }

    //拆分数据,每次发送最大65535字节
    std::vector<std::string> split_data;
    size_t data_size = data.size();
    size_t split_size = 65535;  
    size_t split_count = data_size / split_size;
    size_t split_last_size = data_size % split_size;
    for (size_t i = 0; i < split_count; i++) {
        split_data.emplace_back(data.substr(i * split_size, split_size));
    }
    if (split_last_size > 0) {
        split_data.emplace_back(data.substr(split_count * split_size, split_last_size));
    }
#ifdef NSWEB_DEBUG
    std::cout<< "split_data.size() = " << split_data.size() << std::endl;
#endif

    wslay_frame_iocb frame;
    std::vector<wslay_frame_iocb> send_queue_now;
    for (int i = 0; i < split_data.size(); ++i) {
        
        if(i == split_data.size() - 1){
            frame.fin = 1;
        }else{
            frame.fin = 0;
        }

        if (i == 0) {
            frame.opcode = WSLAY_BINARY_FRAME;
        } else {
            frame.opcode = WSLAY_CONTINUATION_FRAME;
        }

        frame.rsv = 0;
        frame.payload_length = split_data[i].size();
        frame.mask = 1;
        frame.data = (const uint8_t *)split_data[i].c_str();
        frame.data_length = split_data[i].size();
        send_queue_now.emplace_back(frame);
    }
#ifdef NSWEB_DEBUG
        std::cout<< "send_queue.size() = " << send_queue_now.size() << std::endl;
#endif
        std::unique_lock<std::mutex> lock(ws_mutex_queue);
        for(auto &it : send_queue_now){
            send_queue_.emplace(it);
        }
        ws_send_cv.notify_one();
        lock.unlock();
#ifdef NSWEB_DEBUG
    std::cout<< "!is_send_queue_empty(): " << !is_send_queue_empty() << std::endl;
#endif
    return true;
}

//设置接收到消息的回调函数
void websocket_client::set_onmessage_callback(std::function<void(const std::string&)> callback) {
    onmessage_callback_ = callback;
}


// Private methods

// 处理 libcurl 的 HTTP 连接
// websocket 升级握手 
bool websocket_client::perform_handshake(const std::string& url) {

    // 生成随机的 Sec-websocket-Key
    std::string sec_websocket_key { generate_websocket_key() };
    std::string ws_to_http = websocket_url_to_http(url);
    std::string sub = http_to_sub(ws_to_http);

    std::string http_url = http_sub_url(ws_to_http);
    std::string host_url = http_to_host(http_url);

    // 构造 HTTP 升级请求头
    std::string header_ = "GET "+ sub +" HTTP/1.1\r\n"
                        "Host: " + host_url + "\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Key: " + sec_websocket_key + "\r\n"
                        "Sec-WebSocket-Version: 13\r\n"
                        "\r\n"; // 注意最后一行的额外 \r\n
#ifdef NSWEB_DEBUG
    std::cout << "header_: " << header_;
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);// 设置 libcurl 的 HTTP 请求调试选项
#endif
    curl_easy_setopt(curl_, CURLOPT_URL, http_url.c_str());// 设置 libcurl 的 HTTP 请求 URL
    curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 1L);// 设置 libcurl 的 HTTP 请求连接选项

    // 执行 CONNECT 请求
    CURLcode result = curl_easy_perform(curl_);
    if (result != CURLE_OK) {
#ifdef NSWEB_DEBUG
        std::cerr << "curl_easy_perform() failed: " 
                    << curl_easy_strerror(result) << std::endl;
#endif
        return false;
    }

    // 发送 HTTP 升级请求
    size_t sent;
    result = curl_easy_send(curl_, header_.c_str(), header_.length(), &sent);
    if (result != CURLE_OK) {
#ifdef NSWEB_DEBUG
        std::cerr << "send curl_easy_send() failed: " 
                    << curl_easy_strerror(result) << std::endl;
#endif
        return false;
    }

    curl_socket_t sockfd;
    result = curl_easy_getinfo(curl_, CURLINFO_ACTIVESOCKET, &sockfd);
    if (result != CURLE_OK || sockfd == CURL_SOCKET_BAD) {
#ifdef NSWEB_DEBUG
        std::cerr << "curl_easy_getinfo() failed: " 
                    << curl_easy_strerror(result) << std::endl;
#endif
        return false;
    }

    // 设置套接字缓冲区大小
    if (!set_socket_buffer_size(sockfd, 1024 * 1024)) {
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to set socket buffer size." << std::endl;
#endif
        return false;
    }

    // 等待套接字准备好接收
    int select_result = wait_socket(sockfd, 3000);
    if (select_result <= 0) {
#ifdef NSWEB_DEBUG
        if (select_result == 0) {
            std::cerr << "select() timeout." << std::endl;
        } else {
            std::cerr << "select() failed." << std::endl;
        }
#endif
        return false;
    }

    // 读取并验证 101 状态码
    char buffer[256];
    size_t received;

    result = curl_easy_recv(curl_, buffer, sizeof(buffer) - 1, &received);
    if (result != CURLE_OK) {
#ifdef NSWEB_DEBUG
        std::cerr << "revc curl_easy_recv() failed: " 
                    << curl_easy_strerror(result) << std::endl;
#endif
        return false;
    }
    buffer[received] = 0;

    // 查找状态码起始位置
    const char *status_code_start = strstr(buffer, "HTTP/1.1");
    if (status_code_start == nullptr) {
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to find status code." << std::endl;
#endif
        return false;
    }

    // 提取状态码
    std::string status_code(status_code_start + 9, status_code_start + 12);

    if (received <= 0 || strncmp(buffer, "HTTP/1.1 101", 12) != 0) {
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to receive 101 status code. Status code received: "
                    << status_code << std::endl;
#endif
        return false;
    }

    // 查找 Sec-WebSocket-Accept
    const char *sec_websocket_accept_start = strstr(buffer, "Sec-WebSocket-Accept: ");
    if (sec_websocket_accept_start == nullptr) {
#ifdef NSWEB_DEBUG
        std::cerr << "Failed to find Sec-WebSocket-Accept." << std::endl;
#endif
        return false;
    }

    // 提取 Sec-WebSocket-Accept
    std::string sec_websocket_accept(sec_websocket_accept_start + 22, 
                                    sec_websocket_accept_start + 55);

    // 计算期望的 Sec-WebSocket-Accept
    std::string expected_sec_websocket_accept = generate_websocket_accept(sec_websocket_key);
    
    // 验证 Sec-WebSocket-Accept
    if (sec_websocket_accept.find(expected_sec_websocket_accept) == std::string::npos ) {
#ifdef NSWEB_DEBUG
        std::cerr << "Sec-WebSocket-Accept mismatch." << std::endl;
#endif
        return false;
    }

    return true;
}

bool websocket_client::is_send_queue_empty(){
    std::unique_lock<std::mutex> lock(ws_mutex_queue);
    return send_queue_.empty();
}

void websocket_client::websocket_client_thread_send(){
    while (is_connected()) {
        if (is_send_queue_empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::unique_lock<std::mutex> lock(ws_mutex_queue);

        //ws_send_cv.wait(lock, [this] { return !is_send_queue_empty(); });
        // 从队列中取出一帧
        wslay_frame_iocb frame = send_queue_.front();

        int wslay_send_result;
        do {
            std::unique_lock<std::mutex> lock1(ws_mutex_curl);
            wslay_send_result = wslay_frame_send(wslay_frame_ctx_, &frame);
            if (wslay_send_result == WSLAY_ERR_WOULDBLOCK) {
                std::this_thread::yield(); // 添加 yield 以允许其他任务执行
            }
        } while (wslay_send_result == WSLAY_ERR_WOULDBLOCK && !is_send_queue_empty() && is_connected());
#ifdef NSWEB_DEBUG
        switch (wslay_send_result) {
            case 0:
                break;
            case WSLAY_ERR_CALLBACK_FAILURE:
                std::cerr << "wslay_frame_send() failed: WSLAY_ERR_CALLBACK_FAILURE" << std::endl;
                break;
            case WSLAY_ERR_INVALID_ARGUMENT:
                std::cerr << "wslay_frame_send() failed: WSLAY_ERR_INVALID_ARGUMENT" << std::endl;
                break;
            case WSLAY_ERR_WOULDBLOCK:
                std::cerr << "wslay_frame_send() failed: WSLAY_ERR_WOULDBLOCK" << std::endl;
                break;
            default:
                std::cout << "send success: " << wslay_send_result <<std::endl;
                break;
        }

        //std::cerr << "send: " << frame.data << std::endl;
        std::cerr << "send_queue_size: " << send_queue_.size() << std::endl;
#endif
        send_queue_.pop();
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void websocket_client::websocket_client_thread_recv() {
    while (is_connected()) {
        {
            std::unique_lock<std::mutex> lock(ws_mutex_curl);

            int wslay_recv_result;
            do {
                wslay_recv_result = wslay_event_recv(wslay_ctx_);
                if (wslay_recv_result == WSLAY_ERR_WOULDBLOCK) {
                    std::this_thread::yield(); // 添加 yield 以允许其他任务执行
                }
            } while (wslay_recv_result == WSLAY_ERR_WOULDBLOCK);
            
            lock.unlock();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}


// wslay 设置事件回调
void websocket_client::setup_wslay_event_callbacks() {
    wslay_event_callbacks callbacks;
    std::memset(&callbacks, 0, sizeof(callbacks));
    callbacks.recv_callback = recv_callback;
    callbacks.on_msg_recv_callback = on_msgrecv_callback;
    wslay_event_context_client_init(&wslay_ctx_, &callbacks, this);


    wslay_frame_callbacks callbacks_send;
    std::memset(&callbacks_send, 0, sizeof(callbacks_send));
    callbacks_send.send_callback = send_callback;
    callbacks_send.genmask_callback = gen_mask_callback;
    wslay_frame_context_init(&wslay_frame_ctx_, &callbacks_send, this);
}

int websocket_client::gen_mask_callback(uint8_t *buf, size_t len, void *user_data){
#ifdef NSWEB_DEBUG
    std::cerr<<"gen_mask_callback"<<std::endl;
#endif
    return gen_mask(buf, len);
}

//wslay 回调函数
ssize_t websocket_client::send_callback(const uint8_t* data, size_t len, int flags, void* user_data) {
    websocket_client* ws = reinterpret_cast<websocket_client*>(user_data);
#ifdef NSWEB_DEBUG
    std::cout<<"->send_callback()"<<std::endl;
    std::cout << "data: " << data << std::endl;
    std::cout << "data len: " << len << std::endl;
    std::cout << "data flags: " << flags << std::endl;
#endif
    CURLcode result;
    size_t sent;
    result = curl_easy_send(ws->curl_, data, len, &sent);

    if (result == CURLE_AGAIN) {
        return WSLAY_ERR_WOULDBLOCK;
    } else if (result != CURLE_OK) {
#ifdef NSWEB_DEBUG
        std::cerr << "send_callback curl_easy_send() failed: " << curl_easy_strerror(result) << std::endl;
#endif
        return WSLAY_ERR_CALLBACK_FAILURE;
    }
#ifdef NSWEB_DEBUG
    std::cout << "curl_easy_send() ret: " << result << std::endl;
    std::cout << "curl send success size: " << sent << std::endl;
#endif
    return static_cast<ssize_t>(sent);
}


//wslay 回调函数
ssize_t websocket_client::recv_callback(wslay_event_context_ptr ctx, uint8_t* buf, size_t len, int flags, void* user_data) {
    websocket_client* ws = reinterpret_cast<websocket_client*>(user_data);
/*    
#ifdef NSWEB_DEBUG
    std::cout<<"->recv_callback()"<<std::endl;
    std::cout << "buf: " << buf << std::endl;
    std::cout << "buf len: " << len << std::endl;
    std::cout << "buf flags: " << flags << std::endl;
#endif*/
    CURLcode result;
    size_t sent;
    result = curl_easy_recv(ws->curl_, buf, len, &sent);
    
    if (result == CURLE_AGAIN) {
        return WSLAY_ERR_WOULDBLOCK;
    } else if (result != CURLE_OK) {
/*#ifdef NSWEB_DEBUG
        std::cerr << "recv_callback curl_easy_recv() failed: " << curl_easy_strerror(result) << std::endl;
#endif*/
        return WSLAY_ERR_CALLBACK_FAILURE;
    }
/*#ifdef NSWEB_DEBUG
    std::cout << "curl_easy_recv() ret: " << result << std::endl;
    std::cout << "curl recv success size: " << sent << std::endl;
#endif*/
    return static_cast<ssize_t>(sent);
}


void websocket_client::on_msgrecv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg, void* user_data) {
    if (arg->opcode != WSLAY_TEXT_FRAME) {
        return;
    }

    websocket_client* ws = reinterpret_cast<websocket_client*>(user_data);
    std::string message(reinterpret_cast<const char*>(arg->msg), arg->msg_length);

    if (ws->onmessage_callback_) {
        ws->onmessage_callback_(message);
    }
}

}  // namespace nsweb
