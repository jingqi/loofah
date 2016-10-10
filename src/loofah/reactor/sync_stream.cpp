
#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "sync_stream.h"

#define TAG "sync_steam"

namespace loofah
{

void SyncStream::open(socket_t fd)
{
    _fd = fd;
}

void SyncStream::close()
{
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_fd);
#else
        ::close(_fd);
#endif
}

INETAddr SyncStream::get_peer_addr() const
{
    INETAddr ret;
    struct sockaddr_in& peer = ret.get_sockaddr_in();
#if NUT_PLATFORM_OS_WINDOWS
    int plen = sizeof(peer);
#else
    socklen_t plen = sizeof(peer);
#endif
    if (::getpeername(_fd, (struct sockaddr*)&peer, &plen) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::getpeername(), socketfd %d", _fd);
        return ret;
    }
    return ret;
}

}
