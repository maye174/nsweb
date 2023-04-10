#include "nsweb.hpp"
#include "socket_fun.hpp"
#include "tls_fun.hpp"
#include "url_fun.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>

namespace nsweb {

// Constructor
websocket_client::websocket_client()
    : curl_(nullptr), wslay_ctx_(nullptr), is_connected_(false) {}

// Destructor
websocket_client::~websocket_client() {
    disconnect(true);
}

// Public methods
bool websocket_client::connect(const std::string& url) {
    if (is_connected()) {
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl_ = curl_easy_init();
    
    if (!perform_handshake(url)) {
        std::cerr << "Failed to perform handshake." << std::endl;

        curl_easy_cleanup(curl_);
        //curl_global_cleanup();
        return false;
    }
    setup_wslay_event_callbacks();
    is_connected_.store(true, std::memory_order_release);
    ws_thread_ = std::thread(&websocket_client::websocket_client_thread, this);
    return true;
}

void websocket_client::disconnect(bool clear_curl) {
    if (!is_connected()) {
        return;
    }

    is_connected_.store(false, std::memory_order_release);
    curl_easy_cleanup(curl_);
    if (clear_curl) {
        curl_global_cleanup();
    }

    if (ws_thread_.joinable()) {
        ws_thread_.join();
    }

    wslay_event_context_free(wslay_ctx_);
}

bool websocket_client::is_connected() const {
    return is_connected_.load(std::memory_order_acquire);
}

bool websocket_client::send(const std::string data) {
    if (!is_connected()) {
        return false;
    }

    //加锁
    std::unique_lock<std::mutex> lock(ws_mutex_);

    wslay_event_msg msg;
    msg.opcode = WSLAY_BINARY_FRAME;//二进制数据
    msg.msg = reinterpret_cast<const uint8_t*>(data.c_str());
    msg.msg_length = data.size();

    if (wslay_event_queue_msg(wslay_ctx_, &msg) != 0) {
        lock.unlock();
        return false;
    }
    std::cout << "wslay_ctx_ address: " << wslay_ctx_ << std::endl;
    std::cout << "Before wslay_event_send()" << std::endl;
    int send_result = wslay_event_send(wslay_ctx_);
    std::cout << "After wslay_event_send()" << std::endl;
    lock.unlock();
    //释放锁

    if (send_result != 0) {
        std::cerr << "wslay_event_send() failed: " << send_result << std::endl;
        return false;
    }
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

    std::cout << "header_: " << header_;
    
    curl_easy_setopt(curl_, CURLOPT_URL, http_url.c_str());// 设置 libcurl 的 HTTP 请求 URL
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);// 设置 libcurl 的 HTTP 请求调试选项
    curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 1L);// 设置 libcurl 的 HTTP 请求连接选项

    // 执行 CONNECT 请求
    CURLcode result = curl_easy_perform(curl_);
    if (result != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " 
                    << curl_easy_strerror(result) << std::endl;
        return false;
    }

    // 发送 HTTP 升级请求
    size_t sent;
    result = curl_easy_send(curl_, header_.c_str(), header_.length(), &sent);
    if (result != CURLE_OK) {
        std::cerr << "send curl_easy_send() failed: " 
                    << curl_easy_strerror(result) << std::endl;
        return false;
    }

    curl_socket_t sockfd;
    result = curl_easy_getinfo(curl_, CURLINFO_ACTIVESOCKET, &sockfd);
    if (result != CURLE_OK || sockfd == CURL_SOCKET_BAD) {
        std::cerr << "curl_easy_getinfo() failed: " 
                    << curl_easy_strerror(result) << std::endl;
        return false;
    }

    // 设置套接字缓冲区大小
    if (!set_socket_buffer_size(sockfd, 1024 * 1024)) {
        std::cerr << "Failed to set socket buffer size." << std::endl;
        return false;
    }

    // 等待套接字准备好接收
    int select_result = wait_socket(sockfd, 3000);
    if (select_result <= 0) {
        if (select_result == 0) {
            std::cerr << "select() timeout." << std::endl;
        } else {
            std::cerr << "select() failed." << std::endl;
        }
        return false;
    }

    // 读取并验证 101 状态码
    char buffer[256];
    size_t received;

    result = curl_easy_recv(curl_, buffer, sizeof(buffer) - 1, &received);
    if (result != CURLE_OK) {
        std::cerr << "revc curl_easy_recv() failed: " 
                    << curl_easy_strerror(result) << std::endl;
        return false;
    }
    buffer[received] = 0;

    // 查找状态码起始位置
    const char *status_code_start = strstr(buffer, "HTTP/1.1");
    if (status_code_start == nullptr) {
        std::cerr << "Failed to find status code." << std::endl;
        return false;
    }

    // 提取状态码
    std::string status_code(status_code_start + 9, status_code_start + 12);

    if (received <= 0 || strncmp(buffer, "HTTP/1.1 101", 12) != 0) {
        std::cerr << "Failed to receive 101 status code. Status code received: "
                    << status_code << std::endl;
        return false;
    }

    // 查找 Sec-WebSocket-Accept
    const char *sec_websocket_accept_start = strstr(buffer, "Sec-WebSocket-Accept: ");
    if (sec_websocket_accept_start == nullptr) {
        std::cerr << "Failed to find Sec-WebSocket-Accept." << std::endl;
        return false;
    }

    // 提取 Sec-WebSocket-Accept
    std::string sec_websocket_accept(sec_websocket_accept_start + 22, 
                                    sec_websocket_accept_start + 55);

    // 计算期望的 Sec-WebSocket-Accept
    std::string expected_sec_websocket_accept = generate_websocket_accept(sec_websocket_key);
    
    // 验证 Sec-WebSocket-Accept
    if (sec_websocket_accept.find(expected_sec_websocket_accept) == std::string::npos ) {
        std::cerr << "Sec-WebSocket-Accept mismatch." << std::endl;
        return false;
    }

    return true;
}

//curl事件回调
size_t websocket_client::write_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::string* response = reinterpret_cast<std::string*>(userdata);
    response->append(buffer, size * nitems);
    return size * nitems;
}

size_t websocket_client::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::string* response = reinterpret_cast<std::string*>(userdata);
    response->append(buffer, size * nitems);
    return size * nitems;
}

//
void websocket_client::websocket_client_thread() {
    while (is_connected()) {
        {
            std::unique_lock<std::mutex> lock(ws_mutex_);

            int wslay_recv_result;
            do {
                wslay_recv_result = wslay_event_recv(wslay_ctx_);
                if (wslay_recv_result == WSLAY_ERR_WOULDBLOCK) {
                    std::this_thread::yield(); // 添加 yield 以允许其他任务执行
                }
            } while (wslay_recv_result == WSLAY_ERR_WOULDBLOCK);

            int wslay_send_result;
            do {
                wslay_send_result = wslay_event_send(wslay_ctx_);
                if (wslay_send_result == WSLAY_ERR_WOULDBLOCK) {
                    std::this_thread::yield(); // 添加 yield 以允许其他任务执行
                }
            } while (wslay_send_result == WSLAY_ERR_WOULDBLOCK);

            lock.unlock();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}


// wslay 设置事件回调
void websocket_client::setup_wslay_event_callbacks() {
    wslay_event_callbacks callbacks;
    std::memset(&callbacks, 0, sizeof(callbacks));
    callbacks.send_callback = send_callback;
    callbacks.recv_callback = recv_callback;
    callbacks.on_msg_recv_callback = on_msgrecv_callback;

    wslay_event_context_client_init(&wslay_ctx_, &callbacks, this);

}


//wslay 回调函数
ssize_t websocket_client::send_callback(wslay_event_context_ptr ctx, const uint8_t* data, size_t len, int flags, void* user_data) {
    websocket_client* ws = reinterpret_cast<websocket_client*>(user_data);

    CURLcode result;
    size_t sent;
    std::cout << "Before curl_easy_send()" << std::endl;
    result = curl_easy_send(ws->curl_, data, len, &sent);
    std::cout << "After curl_easy_send()" << std::endl;

    if (result == CURLE_AGAIN) {
        return WSLAY_ERR_WOULDBLOCK;
    } else if (result != CURLE_OK) {
        std::cerr << "send_callback curl_easy_send() failed: " << curl_easy_strerror(result) << std::endl;
        return WSLAY_ERR_CALLBACK_FAILURE;
    }

    return static_cast<ssize_t>(sent);
}


//wslay 回调函数
ssize_t websocket_client::recv_callback(wslay_event_context_ptr ctx, uint8_t* buf, size_t len, int flags, void* user_data) {
    websocket_client* ws = reinterpret_cast<websocket_client*>(user_data);

    CURLcode result;
    size_t sent;
    result = curl_easy_recv(ws->curl_, buf, len, &sent);
    
    if (result == CURLE_AGAIN) {
        return WSLAY_ERR_WOULDBLOCK;
    } else if (result != CURLE_OK) {
        std::cerr << "recv_callback curl_easy_recv() failed: " << curl_easy_strerror(result) << std::endl;
        return WSLAY_ERR_CALLBACK_FAILURE;
    }
    
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
