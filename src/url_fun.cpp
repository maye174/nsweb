#include "url_fun.hpp"

namespace nsweb {

std::string http_to_host(const std::string& http_url) {
    std::string host;
    if (http_url.substr(0, 7) == "http://") {
        host = http_url.substr(7);
    } else if (http_url.substr(0, 8) == "https://") {
        host = http_url.substr(8);
    } else {
        host = http_url;
    }
    return host;
}

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