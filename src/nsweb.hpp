

#pragma once

#include <queue>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <curl/curl.h>
#include <wslay/wslay.h>


namespace nsweb {
    class websocket_client {
    public:
        websocket_client();
        ~websocket_client();

        bool connect(const std::string& url);
        void disconnect(bool clear_curl);
        bool is_connected() const;

        bool send(const std::string &data);
        void set_onmessage_callback(std::function<void(const std::string&)> callback);

    private:
        static int gen_mask_callback(uint8_t *buf, size_t len, void *user_data);
        static ssize_t send_callback(const uint8_t* data, size_t len, int flags, void* user_data);

        static ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t* buf, size_t len, int flags, void* user_data);
        static void on_msgrecv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg, void* user_data);

        bool perform_handshake(const std::string& url);
        void setup_wslay_event_callbacks();

        void websocket_client_thread_send();
        void websocket_client_thread_recv();

        bool is_send_queue_empty();

        CURL* curl_;
        std::queue<wslay_frame_iocb> send_queue_;
        wslay_event_context_ptr wslay_ctx_;
        wslay_frame_context_ptr wslay_frame_ctx_;
        //锁
        std::mutex ws_mutex_queue;
        std::mutex ws_mutex_curl;

        //线程
        std::thread ws_thread_send;
        std::thread ws_thread_recv;


        std::atomic_bool is_connected_;
        std::function<void(const std::string&)> onmessage_callback_;
    };
}