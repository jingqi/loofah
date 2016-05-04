
#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h> // NOTE should include winsock2.h before windows.h
#   include <windows.h>
#else
#   include <arpa/inet.h> // for inet_aton()
#endif

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

#if NUT_PLATFORM_OS_WINDOWS
    _sock_addr.sin_addr.S_un.S_addr = ::inet_addr(ip);
#else
    if (::inet_aton(ip, &_sock_addr.sin_addr) == 0) // 0 means invalid
        NUT_LOG_E(TAG, "failed to call ::inet_aton() with ip: %s", ip);
#endif
}

}
