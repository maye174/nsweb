#include "tls_fun.hpp"

#include <random>
#include <cstring>
#include <mutex>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
std::mutex mb_mutex;
namespace nsweb {

bool init_mbedtls() {
    // 初始化 mbedtls 库的熵源上下文
    mbedtls_entropy_init(&entropy);

    // 初始化 mbedtls 库的随机数生成器上下文
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // 设置随机数生成器的种子
    const char *pers = "nsweb_init_mbedtls";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return false;
    }

    return true;
}

void deinit_mbedtls() {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

int gen_mask(uint8_t *buf, size_t len) {
    // 生成随机掩码密钥
    std::unique_lock<std::mutex> lock(mb_mutex);
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg, buf, len);
    lock.unlock();
    if (ret != 0) {
        return -1; // 错误情况下返回非零值
    }
    return 0; // 成功情况下返回零值
}

std::string generate_websocket_key() {
    // 生成随机数
    unsigned char buf[16];
    std::unique_lock<std::mutex> lock(mb_mutex);
    mbedtls_ctr_drbg_random(&ctr_drbg, buf, sizeof(buf));
    lock.unlock();

    // Base64编码
    size_t olen = 0;
    char b64_buf[32];
    mbedtls_base64_encode((unsigned char*)b64_buf, sizeof(b64_buf), &olen, buf, sizeof(buf));
    std::string key(b64_buf, olen);

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