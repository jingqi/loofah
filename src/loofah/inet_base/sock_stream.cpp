
#include "../loofah_config.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "sock_stream.h"
#include "error.h"


#define TAG "loofah.inet_base.sock_stream"

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

socket_t SockStream::get_socket() const
{
    return _socket_fd;
}

bool SockStream::is_null() const
{
    return LOOFAH_INVALID_SOCKET_FD == _socket_fd;
}

bool SockStream::is_valid() const
{
    if (LOOFAH_INVALID_SOCKET_FD == _socket_fd)
        return false;
    return SockOperation::is_valid(_socket_fd);
}

int SockStream::get_last_error() const
{
    const int err = SockOperation::get_last_error(_socket_fd);
    if (0 == err)
        return err;
    return from_errno(err);
}

bool SockStream::shutdown_read()
{
    assert(LOOFAH_INVALID_SOCKET_FD != _socket_fd);
    if (!_reading_shutdown)
        _reading_shutdown = SockOperation::shutdown_read(_socket_fd);
    return _reading_shutdown;
}

bool SockStream::is_reading_shutdown() const
{
    return _reading_shutdown;
}

void SockStream::mark_reading_shutdown()
{
    _reading_shutdown = true;
}

bool SockStream::shutdown_write()
{
    assert(LOOFAH_INVALID_SOCKET_FD != _socket_fd);
    if (!_writing_shutdown)
        _writing_shutdown = SockOperation::shutdown_write(_socket_fd);
    return _writing_shutdown;
}

bool SockStream::is_writing_shutdown() const
{
    return _writing_shutdown;
}

void SockStream::mark_writing_shutdown()
{
    _writing_shutdown = true;
}

ssize_t SockStream::read(void *buf, size_t max_len)
{
    return SockOperation::read(_socket_fd, buf, max_len);
}

ssize_t SockStream::readv(void* const *buf_ptrs, const size_t *len_ptrs, size_t buf_count)
{
    return SockOperation::readv(_socket_fd, buf_ptrs, len_ptrs, buf_count);
}

ssize_t SockStream::write(const void *buf, size_t max_len)
{
    return SockOperation::write(_socket_fd, buf, max_len);
}

ssize_t SockStream::writev(const void* const *buf_ptrs, const size_t *len_ptrs, size_t buf_count)
{
    return SockOperation::writev(_socket_fd, buf_ptrs, len_ptrs, buf_count);
}

InetAddr SockStream::get_local_addr() const
{
    return SockOperation::get_local_addr(_socket_fd);
}

InetAddr SockStream::get_peer_addr() const
{
    return SockOperation::get_peer_addr(_socket_fd);
}

bool SockStream::is_self_connected() const
{
    return SockOperation::is_self_connected(_socket_fd);
}

bool SockStream::set_tcp_nodelay(bool no_delay)
{
    return SockOperation::set_tcp_nodelay(_socket_fd, no_delay);
}

}
