#include "tls_fun.hpp"

#include <random>
#include <mbedtls/sha1.h>
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
    // 固定的GUID字符串
    const std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // 拼接字符串
    std::string concat = sec_websocket_key + guid;

    // 计算SHA-1哈希值
    unsigned char hash[20];
    mbedtls_sha1((unsigned char*)concat.c_str(), concat.length(), hash);

    // Base64编码
    size_t olen = 0;
    char b64_buf[32];
    mbedtls_base64_encode((unsigned char*)b64_buf, sizeof(b64_buf), &olen, hash, sizeof(hash));
    std::string accept(b64_buf, olen);

    return accept;
}

}