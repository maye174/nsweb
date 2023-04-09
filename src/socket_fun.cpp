#include "socket_fun.hpp"

#ifdef __SWITCH__
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
typedef struct fd_set {
  uint32_t fds_bits[__DARWIN_howmany(FD_SETSIZE, __DARWIN_NFDBITS)];
} fd_set;
#else
#include <sys/select.h>
#endif

#include <sys/socket.h>

namespace nsweb {

//设置缓冲区大小
bool set_socket_buffer_size(curl_socket_t sockfd, int size) {
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size));
    if (ret != 0) {
        return false;
    }

    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size));
    if (ret != 0) {
        return false;
    }

    return true;
}

int wait_socket(curl_socket_t sockfd, int timeout_ms) {
    fd_set fdread;
    struct timeval timeout;

    FD_ZERO(&fdread);
    FD_SET(sockfd, &fdread);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(sockfd + 1, &fdread, nullptr, nullptr, &timeout);
    if (ret > 0) {
        return true;
    } else {
        return ret;
    }
}

}