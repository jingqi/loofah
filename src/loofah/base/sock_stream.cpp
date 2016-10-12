
#include "sock_stream.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>


#define TAG "sock_stream"

namespace loofah
{

void SockStream::open(socket_t fd)
{
    assert(INVALID_SOCKET_VALUE != fd && INVALID_SOCKET_VALUE == _socket_fd);
    _socket_fd = fd;
}

void SockStream::close()
{
#if NUT_PLATFORM_OS_WINDOWS
    ::closesocket(_socket_fd);
#else
    ::close(_socket_fd);
#endif
    _socket_fd = INVALID_SOCKET_VALUE;
}

INETAddr SockStream::get_peer_addr() const
{
    INETAddr ret;
    struct sockaddr_in& peer = ret.get_sockaddr_in();
#if NUT_PLATFORM_OS_WINDOWS
    int plen = sizeof(peer);
#else
    socklen_t plen = sizeof(peer);
#endif
    if (::getpeername(_socket_fd, (struct sockaddr*)&peer, &plen) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::getpeername()");
        return ret;
    }
    return ret;
}

}
