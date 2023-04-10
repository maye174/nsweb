#include "tls_fun.hpp"

#include <random>
#include <mbedtls/md5.h>
#include <mbedtls/base64.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

namespace nsweb {

std::string generate_websocket_key() {
    // 初始化随机数生成器
    mbedtls_ctr_drbg_context drbg;
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, NULL, 0);

    // 生成随机数
    unsigned char buf[16];
    mbedtls_ctr_drbg_random(&drbg, buf, sizeof(buf));

    // Base64编码
    size_t olen = 0;
    char b64_buf[32];
    mbedtls_base64_encode((unsigned char*)b64_buf, sizeof(b64_buf), &olen, buf, sizeof(buf));
    std::string key(b64_buf, olen);

    // 清理资源
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    return key;
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