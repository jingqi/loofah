﻿/**
 * 地址相关的几个结构体定义
 *
 * struct sockaddr { // 总共 16 字节
 *     sa_family_t sa_family;      // Address family, AF_INET / AF_INET6
 *     char        sa_data[14];    // Others
 * };
 *
 * struct sockaddr_in { // 总共 16 字节
 *     sa_family_t           sin_family;      // AF_INET
 *     unsigned short int    sin_port;        // Port
 *     struct in_addr        sin_addr;        // Internet address
 *
 *     // Pad to size of `struct sockaddr'.
 *     unsigned char __pad[__SOCK_SIZE__ - sizeof(short int) -
 *                         sizeof(unsigned short int) - sizeof(struct in_addr)];
 * };
 *
 * struct sockaddr_in6 { // 总共 28 字节
 *     unsigned short int      sin6_family;    // AF_INET6
 *     __u16                   sin6_port;      // Transport layer port
 *     __u32                   sin6_flowinfo;  // IPv6 flow information
 *     struct in6_addr         sin6_addr;      // IPv6 address
 *     __u32                   sin6_scope_id;  // scope id (new in RFC2553)
 * };
 *
 */

#include <assert.h>
#include <string.h>
#include <algorithm> // for std::max()

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h> // NOTE should include winsock2.h before windows.h
#   include <ws2tcpip.h> // For ::inet_pton(), ::inet_ntop()
#   include <windows.h>
#else
#   include <arpa/inet.h> // For ::inet_aton()
#   include <netdb.h> // For ::gethostbyname(), ::getaddrinfo()
#endif

#include <nut/platform/endian.h>
#include <nut/logging/logger.h>
#include <nut/util/string/to_string.h>

#include "inet_addr.h"


#define TAG "loofah.inet_base.inet_addr"

namespace loofah
{

/**
 * Windows/Mingw 下缺少 inet_pton() inet_ntop() 函数
 * See http://stackoverflow.com/questions/15660203/inet-pton-identifier-not-found
 */
#if NUT_PLATFORM_OS_WINDOWS && NUT_PLATFORM_CC_MINGW
static int inet_pton(int af, const char *src, void *dst) noexcept
{
    /* Stupid non-const API */
    const size_t maxlen = std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN); // Include tailing '\0' already
    char src_copy[maxlen];
    ::strncpy(src_copy, src, maxlen);
    src_copy[maxlen - 1] = 0;

    struct sockaddr_storage ss;
    ::memset(&ss, 0, sizeof(ss));

    int size = (int) sizeof(ss);
    if (0 == ::WSAStringToAddressA(src_copy, af, nullptr, (struct sockaddr *)&ss, &size))
    {
        switch(af)
        {
        case AF_INET:
            *(struct in_addr *) dst = ((struct sockaddr_in *) &ss)->sin_addr;
            return 1;

        case AF_INET6:
            *(struct in6_addr *) dst = ((struct sockaddr_in6 *) &ss)->sin6_addr;
            return 1;
        }
    }
    return 0;
}

static const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) noexcept
{
    struct sockaddr_storage ss;
    ::memset(&ss, 0, sizeof(ss));
    ss.ss_family = af;

    switch(af)
    {
    case AF_INET:
        ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
        break;

    case AF_INET6:
        ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
        break;

    default:
        return nullptr;
    }

    /* Cannot direclty use &size because of strict aliasing rules */
    unsigned long s = size;
    if (0 == ::WSAAddressToStringA((struct sockaddr *) &ss, sizeof(ss), nullptr, dst, &s))
        return dst;
    return nullptr;
}
#endif

InetAddr::InetAddr(int port, bool loopback, bool ipv6) noexcept
{
    assert(((void*) &_sock_addr) == ((void*) &_sock_addr6));

    if (ipv6)
    {
        ::memset(&_sock_addr6, 0, sizeof(_sock_addr6));
        _sock_addr6.sin6_family = AF_INET6;
        if (loopback)
            _sock_addr6.sin6_addr = in6addr_loopback;
        else
            _sock_addr6.sin6_addr = in6addr_any;
        _sock_addr6.sin6_port = htobe16(port);
    }
    else
    {
        ::memset(&_sock_addr, 0, sizeof(_sock_addr));
        _sock_addr.sin_family = AF_INET;
        if (loopback)
            _sock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        else
            _sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        _sock_addr.sin_port = htons(port);
    }
}

InetAddr::InetAddr(const char *addr, int port, bool ipv6) noexcept
{
    assert(nullptr != addr);
    assert(((void*) &_sock_addr) == ((void*) &_sock_addr6));

    if (ipv6)
    {
        ::memset(&_sock_addr6, 0, sizeof(_sock_addr6));
        _sock_addr6.sin6_family = AF_INET6;
        _sock_addr6.sin6_addr = in6addr_any;
        _sock_addr6.sin6_port = htobe16(port);

        // NOTE ::inet_pton() returns:
        //      1 for successed
        //      0 for wrong address format
        //      -1 for system error, errno will be set
        if (inet_pton(AF_INET6, addr, &_sock_addr6.sin6_addr) <= 0)
        {
            struct addrinfo hint, *answer = nullptr;
            ::memset(&hint, 0, sizeof(hint));
            hint.ai_family = AF_INET6;
            hint.ai_socktype = SOCK_STREAM;
            if (0 != ::getaddrinfo(addr, nullptr, &hint, &answer))
            {
                NUT_LOG_E(TAG, "can not resolve addr: \"%s\"", addr);
                if (nullptr != answer)
                    ::freeaddrinfo(answer);
                return;
            }
            for (struct addrinfo *p = answer; nullptr != p; p = p->ai_next)
            {
                _sock_addr6.sin6_addr = ((struct sockaddr_in6*) (p->ai_addr))->sin6_addr;
                break;
            }
            if (nullptr != answer)
                ::freeaddrinfo(answer);
        }
    }
    else
    {
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
            if (nullptr == phostent)
            {
                NUT_LOG_E(TAG, "can not resolve addr: \"%s\"", addr);
                return;
            }
            assert(AF_INET == phostent->h_addrtype);

            _sock_addr.sin_addr.s_addr = *((unsigned long*) phostent->h_addr);
        }
    }

    // NUT_LOG_D(TAG, "resolved ip: \"%s\" -> \"%s\"", addr, ::inet_ntoa(_sock_addr.sin_addr));
}

InetAddr::InetAddr(const struct sockaddr_in& sock_addr) noexcept
{
    assert(((void*) &_sock_addr) == ((void*) &_sock_addr6));
    ::memcpy(&_sock_addr, &sock_addr, sizeof(sock_addr));
}

InetAddr::InetAddr(const struct sockaddr_in6& sock_addr) noexcept
{
    assert(((void*) &_sock_addr) == ((void*) &_sock_addr6));
    ::memcpy(&_sock_addr6, &sock_addr, sizeof(sock_addr));
}

InetAddr::InetAddr(const struct sockaddr* sock_addr) noexcept
{
    assert(nullptr != sock_addr);
    if (AF_INET == sock_addr->sa_family)
        ::memcpy(&_sock_addr, sock_addr, sizeof(_sock_addr));
    else
        ::memcpy(&_sock_addr6, sock_addr, sizeof(_sock_addr6));
}

bool InetAddr::operator==(const InetAddr& addr) const noexcept
{
    if (AF_INET == _sock_addr.sin_family && AF_INET == addr._sock_addr.sin_family)
    {
        return _sock_addr.sin_port == addr._sock_addr.sin_port &&
            _sock_addr.sin_addr.s_addr == addr._sock_addr.sin_addr.s_addr;
    }
    else if (AF_INET6 == _sock_addr6.sin6_family && AF_INET6 == addr._sock_addr6.sin6_family)
    {
        return _sock_addr6.sin6_port == addr._sock_addr6.sin6_port &&
            0 == ::memcmp(&_sock_addr6.sin6_addr, &addr._sock_addr6.sin6_addr,
                          sizeof(_sock_addr6.sin6_addr));
    }
    return false;
}

bool InetAddr::operator!=(const InetAddr& addr) const noexcept
{
    return !(*this == addr);
}

bool InetAddr::is_ipv6() const noexcept
{
    return AF_INET6 == _sock_addr6.sin6_family;
}

struct sockaddr* InetAddr::cast_to_sockaddr() noexcept
{
    return (struct sockaddr*) &_sock_addr;
}

const struct sockaddr* InetAddr::cast_to_sockaddr() const noexcept
{
    return (const struct sockaddr*) &_sock_addr;
}

socklen_t InetAddr::get_max_sockaddr_size() const noexcept
{
    return (std::max)(sizeof(_sock_addr), sizeof(_sock_addr6));
}

socklen_t InetAddr::get_sockaddr_size() const noexcept
{
    return (AF_INET == _sock_addr.sin_family ? sizeof(_sock_addr) : sizeof(_sock_addr6));
}

std::string InetAddr::get_ip() const noexcept
{
    const int buflen = (std::max)(INET_ADDRSTRLEN, INET6_ADDRSTRLEN); // Include tailing '\0' already
    char buf[buflen];
    const int domain = is_ipv6() ? AF_INET6 : AF_INET;
#if NUT_PLATFORM_OS_WINDOWS
    inet_ntop(domain, (void*) cast_to_sockaddr(), buf, buflen);
#else
    ::inet_ntop(domain, cast_to_sockaddr(), buf, buflen);
#endif
    return buf;
}

int InetAddr::get_port() const noexcept
{
    return is_ipv6() ? be16toh(_sock_addr6.sin6_port) : ntohs(_sock_addr.sin_port);
}

std::string InetAddr::to_string() const noexcept
{
    std::string s = get_ip();
    s.push_back(':');
    s += nut::ulong_to_str(get_port());
    return s;
}

}
