
#include "../loofah.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#   include <io.h> // for ::close()
#else
#   include <sys/socket.h> // for ::socket() and so on
#   include <netinet/in.h> // for struct sockaddr_in
#   include <unistd.h> // for ::close()
#endif

#include <nut/logging/logger.h>

#include "connector.h"
#include "../inet_base/utils.h"
#include "../inet_base/sock_base.h"


#define TAG "loofah.connector"

namespace loofah
{

bool Connector::connect(Channel *channel, const InetAddr& address)
{
    assert(NULL != channel);

    // New socket
    socket_t fd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == fd)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // Connect
    const struct sockaddr_in& server_addr = address.get_sockaddr_in();
    const int status = ::connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (-1 == status)
    {
        NUT_LOG_E(TAG, "failed to call ::connect(), socketfd %d, errno %d", fd, errno);
        int save_errno = errno;
        ::close(fd);
        errno = save_errno; // the close() may be error
        return false;
    }

    // Make it nonblocking
    if (!SockBase::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    channel->open(fd);

    return true;
}

}
