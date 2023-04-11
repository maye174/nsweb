#pragma once

#include <string>

namespace nsweb {

bool init_mbedtls();

void deinit_mbedtls();

int gen_mask(uint8_t *buf, size_t len);

std::string generate_websocket_key();

std::string generate_websocket_accept(const std::string& sec_websocket_key);

}