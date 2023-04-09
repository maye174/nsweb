#pragma once

#include <string>

namespace nsweb {

std::string generate_websocket_key();

std::string generate_websocket_accept(const std::string& sec_websocket_key);

}