#include "url_fun.hpp"

namespace nsweb {

//提取请求头路径
std::string http_to_sub(const std::string& http_url) {
    std::string sub;
    if (http_url.substr(0, 7) == "http://") {
        sub = http_url.substr(7);
    } else if (http_url.substr(0, 8) == "https://") {
        sub = http_url.substr(8);
    } else {
        sub = http_url;
    }
    size_t pos = sub.find('/');
    if (pos != std::string::npos) {
        sub = sub.substr(pos);
    } else {
        sub = "/";
    }

    return sub;
}

//提取请求头host
std::string http_to_host(const std::string& http_url) {
    std::string host;
    if (http_url.substr(0, 7) == "http://") {
        host = http_url.substr(7);
    } else if (http_url.substr(0, 8) == "https://") {
        host = http_url.substr(8);
    } else {
        host = http_url;
    }
    size_t pos = host.find('/');
    if (pos != std::string::npos) {
        host = host.substr(0, pos);
    }

    return host;
}

//去除子路径
std::string http_sub_url(const std::string& http_url) {
    std::string sub_url;
    if (http_url.substr(0, 7) == "http://") {
        sub_url = "http://" + http_to_host(http_url);
    } else if (http_url.substr(0, 8) == "https://") {
        sub_url = "https://" + http_to_host(http_url);
    } else {
        sub_url = http_url;
    }
    return sub_url;
}

//将ws头转换为http头
std::string websocket_url_to_http(const std::string& ws_url) {
    std::string http_url;
    if (ws_url.substr(0, 5) == "ws://") {
        http_url = "http://" + ws_url.substr(5);
    } else if (ws_url.substr(0, 6) == "wss://") {
        http_url = "https://" + ws_url.substr(6);
    } else {
        http_url = ws_url;
    }
    return http_url;
}

}