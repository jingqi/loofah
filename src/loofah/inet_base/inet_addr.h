
#ifndef ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_
#define ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_

#include <string>

#include "../loofah.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <netinet/in.h> // for struct sockaddr_in
#endif


namespace loofah
{

/**
 * 网络地址，一般包含IP地址和端口号
 */
class LOOFAH_API InetAddr
{
    union
    {
        struct sockaddr_in _sock_addr; // for IPv4
        struct sockaddr_in6 _sock_addr6; // for IPv6
    };

public:
    explicit InetAddr(int port = 0, bool loopback = false, bool ipv6 = false);
    InetAddr(const char *addr, int port, bool ipv6 = false);

    // IPv4 address
    InetAddr(const struct sockaddr_in& sock_addr);

    // IPv6 address
    InetAddr(const struct sockaddr_in6& sock_addr);

    bool operator==(const InetAddr& addr) const;

    bool operator!=(const InetAddr& addr) const
    {
        return !(*this == addr);
    }

    const struct sockaddr_in& get_sockaddr_in() const
    {
        return _sock_addr;
    }

    const struct sockaddr_in6& get_sockaddr_in6() const
    {
        return _sock_addr6;
    }

    struct sockaddr_in& get_sockaddr_in()
    {
        return _sock_addr;
    }

    struct sockaddr_in6& get_sockaddr_in6()
    {
        return _sock_addr6;
    }

    std::string to_string() const;
};

}

#endif
