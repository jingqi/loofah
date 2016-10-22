
#include "sock_stream.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>


#define TAG "loofah.sock_stream"

namespace loofah
{

void SockStream::open(socket_t fd)
{
    assert(INVALID_SOCKET_VALUE != fd && INVALID_SOCKET_VALUE == _socket_fd);
    _socket_fd = fd;
}

}
