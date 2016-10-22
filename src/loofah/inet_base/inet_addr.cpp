/**
 * 地址相关的几个结构体定义

   struct sockaddr {
       sa_family_t sa_family;      // address family, AF_xxx
       char        sa_data[14];    // 14 bytes of protocol address
   };

   struct sockaddr_in {
       sa_family_t           sin_family;      // Address family
       unsigned short int    sin_port;        // Port number
       struct in_addr        sin_addr;        // Internet address

       // Pad to size of `struct sockaddr'.
       unsigned char __pad[__SOCK_SIZE__ - sizeof(short int) -
                           sizeof(unsigned short int) - sizeof(struct in_addr)];
   };

   struct sockaddr_in6 {
       unsigned short int      sin6_family;    // AF_INET6
       __u16                   sin6_port;      // Transport layer port
       __u32                   sin6_flowinfo;  // IPv6 flow information
       struct in6_addr         sin6_addr;      // IPv6 address
       __u32                   sin6_scope_id;  // scope id (new in RFC2553)
   };

 *
 */

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

#include <netdb.h> // for ::getaddrinfo()

#include <nut/logging/logger.h>
#include <nut/util/string/to_string.h>

#include "inet_addr.h"
#include "inet_endian.h"

#define TAG "loofah.inet_addr"

namespace loofah
{

InetAddr::InetAddr(int port, bool loopback, bool ipv6)
{
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

InetAddr::InetAddr(const char *addr, int port, bool ipv6)
{
    assert(NULL != addr);

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
        if (::inet_pton(AF_INET6, addr, &_sock_addr6.sin6_addr) <= 0)
        {
            struct addrinfo hint, *answer = NULL;
            ::memset(&hint, 0, sizeof(hint));
            hint.ai_family = AF_INET6;
            hint.ai_socktype = SOCK_STREAM;
            if (0 != ::getaddrinfo(addr, NULL, &hint, &answer))
            {
                NUT_LOG_E(TAG, "can not resolve addr: \"%s\"", addr);
                if (NULL != answer)
                    ::freeaddrinfo(answer);
                return;
            }
            for (struct addrinfo *p = answer; NULL != p; p = p->ai_next)
            {
                _sock_addr6.sin6_addr = ((struct sockaddr_in6*) (p->ai_addr))->sin6_addr;
                break;
            }
            if (NULL != answer)
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
            if (NULL == phostent)
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

InetAddr::InetAddr(const struct sockaddr_in& sock_addr)
{
    ::memcpy(&_sock_addr, &sock_addr, sizeof(sock_addr));
}

InetAddr::InetAddr(const struct sockaddr_in6& sock_addr)
{
    ::memcpy(&_sock_addr6, &sock_addr, sizeof(sock_addr));
}

bool InetAddr::operator==(const InetAddr& addr) const
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

std::string InetAddr::to_string() const
{
    std::string s;
    s += ::inet_ntoa(_sock_addr.sin_addr);
    s.push_back(':');
    s += nut::ulong_to_str(ntohs(_sock_addr.sin_port));
    return s;
}

}
