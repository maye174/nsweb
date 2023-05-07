#include "danmaku_live.h"
#include <chrono>
#include <thread>
#include <iostream>

int main() {
    LiveDanmaku liveDanmaku;
    liveDanmaku.connect(5050, 0);
    std::cout<<"sleeping"<<std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(90));  
    liveDanmaku.disconnect();
    std::cout<<"close"<<std::endl;
    return 0;
}