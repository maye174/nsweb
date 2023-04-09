#pragma once

#include <curl/curl.h>

namespace nsweb {

bool set_socket_buffer_size(curl_socket_t sockfd, int size);

int wait_socket(curl_socket_t sockfd, int timeout_ms);

}