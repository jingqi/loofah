
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
class LOOFAH_API INETAddr
{
    struct sockaddr_in _sock_addr;

public:
    explicit INETAddr(int port = 0);
    INETAddr(const char *addr, int port);
    INETAddr(const struct sockaddr_in& sock_addr);

    const struct sockaddr_in& get_sockaddr_in() const
    {
        return _sock_addr;
    }

    struct sockaddr_in& get_sockaddr_in()
    {
        return _sock_addr;
    }

    std::string to_string() const;
};

}

#endif
