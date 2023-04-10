#pragma once

#include <string>

namespace nsweb{

std::string http_to_sub(const std::string& http_url);

std::string http_to_host(const std::string& http_url);

std::string http_sub_url(const std::string& http_url);

std::string websocket_url_to_http(const std::string& ws_url);

}

