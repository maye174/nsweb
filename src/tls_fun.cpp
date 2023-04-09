#include "tls_fun.hpp"

#include <random>
#include <mbedtls/md5.h>
#include <mbedtls/base64.h>

namespace nsweb {

std::string generate_websocket_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
        key[i] = dis(gen);
    }

    size_t output_size = 0;
    mbedtls_base64_encode(nullptr, 0, &output_size, key, sizeof(key));
    std::string output(output_size, 0);
    mbedtls_base64_encode((unsigned char*) output.c_str(), output.size(), &output_size, key, sizeof(key));

    return output;
}

std::string generate_websocket_accept(const std::string& sec_websocket_key) {
    std::string concatenated = sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t hash[16];
    mbedtls_md5((const uint8_t*) concatenated.c_str(), concatenated.length(), hash);

    size_t output_size = 0;
    mbedtls_base64_encode(nullptr, 0, &output_size, hash, sizeof(hash));
    std::string output(output_size, 0);
    mbedtls_base64_encode((unsigned char*) &output[0], output.size(), &output_size, hash, sizeof(hash));

    return output;
}

}