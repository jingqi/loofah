
#ifndef ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_
#define ___HEADFILE_B0333459_F9CF_4935_A74F_41D0071CE268_

#include <nut/platform/platform.h>

#include <netinet/in.h> // for struct sockaddr_in

namespace loofah
{

class INETAddr
{
    struct sockaddr_in _sock_addr;

public:
    INETAddr(const char *ip, int port);

    const struct sockaddr_in& get_sockaddr_in() const
    {
        return _sock_addr;
    }
};

}

#endif
