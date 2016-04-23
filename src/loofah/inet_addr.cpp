
#include <assert.h>
#include <string.h>
#include <arpa/inet.h> // for inet_aton()

#include <nut/logging/logger.h>

#include "inet_addr.h"

#define TAG "loofah.inet_addr"

namespace loofah
{

INETAddr::INETAddr(const char *ip, int port)
{
    assert(NULL != ip);

    ::memset(&_sock_addr, 0, sizeof(_sock_addr));
    _sock_addr.sin_family = AF_INET;
    _sock_addr.sin_port = htons(port);

    if (::inet_aton(ip, &_sock_addr.sin_addr) == 0) // 0 means invalid
        NUT_LOG_E(TAG, "failed to call ::inet_aton() with ip: %s", ip);
}

}
