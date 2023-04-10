#include "nsweb.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // 创建 WebSocket 客户端实例
    nsweb::websocket_client ws_client;
    // 设置 onmessage 回调，以便在接收到服务器消息时进行处理
    ws_client.set_onmessage_callback([](const std::string &message) {
        std::cout << "Received message from server: " << message << std::endl;
    });
    //http://121.40.165.18:8800
    // 连接到 WebSocket 服务器（这里使用了一个提供回显服务的服务器）
    /**/
    if (ws_client.connect("ws://121.40.165.18:8800"/*"wss://broadcastlv.chat.bilibili.com/sub"*/)) {
        std::cout << "Connected to server." << std::endl;

        //std::this_thread::sleep_for(std::chrono::seconds(1));
        // 向服务器发送一条消息
        std::string message = "<span style='color:red;'>任何消息</span>";
        if (ws_client.send(message)) {
            std::cout << "Sent message to server: " << message << std::endl;
        } else {
            std::cerr << "Failed to send message." << std::endl;
        }

        // 等待一段时间以接收服务器响应
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 断开与服务器的连接
        ws_client.disconnect(true);
        std::cout << "Disconnected from server." << std::endl;
    } else {
        std::cerr << "Failed to connect to server." << std::endl;
    }

    return 0;
}
