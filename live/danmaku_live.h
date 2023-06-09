
//
// Created by maye174 on 2023/4/6.
//

#pragma once


#include <string>
#include <atomic>
#include <vector>
//#include <thread>
#include <mutex>

//#define MG_ARCH 14
#define MG_ENABLE_HTTP 1
#define MG_ENABLE_HTTP_WEBSOCKET 1
#include "mongoose.h"  // Include Mongoose header file

class LiveDanmaku {
public:
    int room_id;
    int uid;
    void connect(int room_id, int uid);
    void disconnect();

    void send_join_request(int room_id, int uid);
    void send_heartbeat();
    void send_text_message(const std::string &message);
    void onMessage(std::string message);

    bool is_connected();

    LiveDanmaku(); 
    ~LiveDanmaku();
private:
    std::atomic_bool connected{false};
    std::thread th;
    //std::thread mongoose_thread;

    //人气值
    int popularity = 0;

    std::mutex mongoose_mutex;
    mg_mgr *mgr;
    mg_connection *nc;
};
