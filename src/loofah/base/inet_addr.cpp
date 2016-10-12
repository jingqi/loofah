
#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h> // NOTE should include winsock2.h before windows.h
#   include <windows.h>
#else
#   include <arpa/inet.h> // for inet_aton()
#   include <netdb.h> // for gethostbyname()
#endif

#include <nut/logging/logger.h>
#include <nut/util/string/to_string.h>

#include "inet_addr.h"

#define TAG "loofah.inet_addr"

namespace loofah
{

INETAddr::INETAddr(int port)
{
    ::memset(&_sock_addr, 0, sizeof(_sock_addr));
    _sock_addr.sin_family = AF_INET;
    _sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    _sock_addr.sin_port = htons(port);
}

INETAddr::INETAddr(const char *addr, int port)
{
    assert(NULL != addr);

    ::memset(&_sock_addr, 0, sizeof(_sock_addr));
    _sock_addr.sin_family = AF_INET;
    _sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    _sock_addr.sin_port = htons(port);

#if NUT_PLATFORM_OS_WINDOWS
    _sock_addr.sin_addr.S_un.S_addr = ::inet_addr(addr);
    if (INADDR_NONE == _sock_addr.sin_addr.S_un.S_addr)
#else
    if (0 == ::inet_aton(addr, &_sock_addr.sin_addr)) // 0 means failed
#endif
    {
        // 传入的不是ip字符串，而是主机名，需要查询主机地址
        struct hostent* phostent = ::gethostbyname(addr);
        if (NULL == phostent)
        {
            NUT_LOG_E(TAG, "can not resolve addr: \"%s\"", addr);
            return;
        }

        _sock_addr.sin_addr.s_addr = *((unsigned long*) phostent->h_addr);
    }

    // NUT_LOG_D(TAG, "resolved ip: \"%s\" -> \"%s\"", addr, ::inet_ntoa(_sock_addr.sin_addr));
}

INETAddr::INETAddr(const struct sockaddr_in& sock_addr)
{
    ::memcpy(&_sock_addr, &sock_addr, sizeof(sock_addr));
}

std::string INETAddr::to_string() const
{
    std::string s;
    s += ::inet_ntoa(_sock_addr.sin_addr);
    s.push_back(':');
    s += nut::ulong_to_str(ntohs(_sock_addr.sin_port));
    return s;
}

}
