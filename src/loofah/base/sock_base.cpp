
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#   include <mswsock.h>
#else
#   include <sys/socket.h> // for ::setsockopt() and so on
#   include <fcntl.h> // for ::fcntl()
#endif

#include "sock_base.h"


namespace loofah
{
    
bool SockBase::make_listen_addr_reuseable(socket_t listener_socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = TRUE;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                             (char*)&optval, sizeof(optval));
#else
    int optval = 1;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEPORT,
                             &optval, sizeof(optval));
#endif
}

bool SockBase::make_nonblocking(socket_t socket_fd, bool nonblocking)
{
#if NUT_PLATFORM_OS_WINDOWS
    unsigned long mode = (nonblocking ? 1 : 0);
    return ::ioctlsocket(socket_fd, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return ::fcntl(socket_fd, F_SETFL, flags) == 0;
#endif
}
    
}