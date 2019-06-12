
#ifndef ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_
#define ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_

#include "../loofah_config.h"

#include <string>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h>
#   include <ws2ipdef.h> // for struct sockaddr_in6
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
public:
    explicit InetAddr(int port = 0, bool loopback = false, bool ipv6 = false) noexcept;
    InetAddr(const char *addr, int port, bool ipv6 = false) noexcept;

    // IPv4 address
    InetAddr(const struct sockaddr_in& sock_addr) noexcept;

    // IPv6 address
    InetAddr(const struct sockaddr_in6& sock_addr) noexcept;

    // IPv4 or IPv6
    InetAddr(const struct sockaddr* sock_addr) noexcept;

    bool operator==(const InetAddr& addr) const noexcept;
    bool operator!=(const InetAddr& addr) const noexcept;

    bool is_ipv6() const noexcept;

    struct sockaddr* cast_to_sockaddr() noexcept;
    const struct sockaddr* cast_to_sockaddr() const noexcept;

    socklen_t get_max_sockaddr_size() const noexcept;

    socklen_t get_sockaddr_size() const noexcept;

    std::string get_ip() const noexcept;
    int get_port() const noexcept;
    std::string to_string() const noexcept;

private:
    union
    {
        struct sockaddr_in _sock_addr; // for IPv4
        struct sockaddr_in6 _sock_addr6; // for IPv6
    };
};

}

#endif
