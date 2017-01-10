
#include "sock_stream.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>


#define TAG "loofah.sock_stream"

namespace loofah
{

SockStream::~SockStream()
{
    if (LOOFAH_INVALID_SOCKET_FD != _socket_fd)
        close();
}

void SockStream::open(socket_t fd)
{
    assert(LOOFAH_INVALID_SOCKET_FD != fd && LOOFAH_INVALID_SOCKET_FD == _socket_fd);
    _socket_fd = fd;
    _reading_shutdown = false;
    _writing_shutdown = false;
}

void SockStream::close()
{
    assert(LOOFAH_INVALID_SOCKET_FD != _socket_fd);
    SockOperation::close(_socket_fd);
    _socket_fd = LOOFAH_INVALID_SOCKET_FD;
    _reading_shutdown = true;
    _writing_shutdown = true;
}

bool SockStream::shutdown_read()
{
    assert(LOOFAH_INVALID_SOCKET_FD != _socket_fd);
    if (!_reading_shutdown)
        _reading_shutdown = SockOperation::shutdown_read(_socket_fd);
    return _reading_shutdown;
}
bool SockStream::shutdown_write()
{
    assert(LOOFAH_INVALID_SOCKET_FD != _socket_fd);
    if (!_writing_shutdown)
        _writing_shutdown = SockOperation::shutdown_write(_socket_fd);
    return _writing_shutdown;
}

}
