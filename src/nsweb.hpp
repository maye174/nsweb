

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
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

        bool send(const std::string& data);
        void set_onmessage_callback(std::function<void(const std::string&)> callback);

    private:
        static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
        static size_t write_callback(char* buffer, size_t size, size_t nitems, void* userdata);
        static ssize_t send_callback(wslay_event_context_ptr ctx, const uint8_t* data, size_t len, int flags, void* user_data);
        static ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t* buf, size_t len, int flags, void* user_data);
        static void on_msgrecv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg, void* user_data);

        bool perform_handshake(const std::string& url);
        void setup_wslay_event_callbacks();
        void websocket_client_thread();

        CURL* curl_;
        wslay_event_context_ptr wslay_ctx_;
        std::thread ws_thread_;
        std::mutex ws_mutex_;
        bool is_connected_;
        std::function<void(const std::string&)> onmessage_callback_;
    };
}