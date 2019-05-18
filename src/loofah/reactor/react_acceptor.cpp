
#include "../loofah_config.h"

#include <assert.h>

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "../inet_base/utils.h"
#include "../inet_base/sock_operation.h"
#include "../inet_base/error.h"
#include "react_acceptor.h"
#include "react_channel.h"


#define TAG "loofah.react_acceptor"

namespace loofah
{

ReactAcceptorBase::~ReactAcceptorBase()
{
    if (LOOFAH_INVALID_SOCKET_FD != _listening_socket)
        SockOperation::close(_listening_socket);
    _listening_socket = LOOFAH_INVALID_SOCKET_FD;
}

bool ReactAcceptorBase::listen(const InetAddr& addr, int listen_num)
{
    // Create socket
    const int domain = addr.is_ipv6() ? AF_INET6 : AF_INET;
    _listening_socket = ::socket(domain, SOCK_STREAM, 0);
    if (LOOFAH_INVALID_SOCKET_FD == _listening_socket)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }

    // Make port reuseable
    if (!SockOperation::set_reuse_addr(_listening_socket))
        NUT_LOG_W(TAG, "failed to make listen socket addr reuseable, socketfd %d", _listening_socket);
    if (!SockOperation::set_reuse_port(_listening_socket))
        NUT_LOG_W(TAG, "failed to make listen socket port reuseable, socketfd %d", _listening_socket);

    // Bind
    if (::bind(_listening_socket, addr.cast_to_sockaddr(), addr.get_sockaddr_size()) < 0)
    {
        LOOFAH_LOG_FD_ERRNO(bind, _listening_socket);
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        SockOperation::close(_listening_socket);
        _listening_socket = LOOFAH_INVALID_SOCKET_FD;
        return false;
    }

    // Listen
    if (::listen(_listening_socket, listen_num) < 0)
    {
        LOOFAH_LOG_FD_ERRNO(listen, _listening_socket);
        NUT_LOG_E(TAG, "failed to call listen() with addr %s", addr.to_string().c_str());
        SockOperation::close(_listening_socket);
        _listening_socket = LOOFAH_INVALID_SOCKET_FD;
        return false;
    }

    // Make socket non-blocking
    if (!SockOperation::set_nonblocking(_listening_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listening_socket);

    return true;
}

socket_t ReactAcceptorBase::get_socket() const
{
    return _listening_socket;
}

void ReactAcceptorBase::handle_accept_ready()
{
    // NOTE 在 edge-trigger 模式下，需要一次接收干净
    while (true)
    {
        // Accept
        const socket_t fd = accept(_listening_socket);
        if (LOOFAH_INVALID_SOCKET_FD == fd)
            break;

        // Create new handler
        nut::rc_ptr<ReactChannel> channel = create_channel();
        channel->initialize();
        channel->open(fd);
        channel->handle_channel_connected();
    }
}

socket_t ReactAcceptorBase::accept(socket_t listening_socket)
{
    InetAddr peer_addr;
    socklen_t rsz = peer_addr.get_max_sockaddr_size();
    socket_t fd = ::accept(listening_socket, peer_addr.cast_to_sockaddr(), &rsz);
    if (LOOFAH_INVALID_SOCKET_FD == fd)
    {
#if NUT_PLATFORM_OS_WINDOWS
        const int errcode = ::WSAGetLastError();
        // NOTE 错误码 WSAEWOULDBLOCK 表示已经没有资源了，等待下次异步通知，是正常的
        if (WSAEWOULDBLOCK != errcode)
            LOOFAH_LOG_FD_ERRNO(accept, listening_socket);
#else
        // NOTE 错误码 EAGAIN 表示已经没有资源了，等待下次异步通知，是正常的
        if (EAGAIN != errno)
            LOOFAH_LOG_FD_ERRNO(accept, listening_socket);
#endif
        return LOOFAH_INVALID_SOCKET_FD;
    }

    if (!SockOperation::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}

void ReactAcceptorBase::handle_connect_ready()
{
    assert(false); // Should not run into this place
}

void ReactAcceptorBase::handle_read_ready()
{
    assert(false); // Should not run into this place
}

void ReactAcceptorBase::handle_write_ready()
{
    assert(false); // Should not run into this place
}

void ReactAcceptorBase::handle_io_error(int err)
{
    NUT_LOG_E(TAG, "fd %d, loofah error raised %d: %s", get_socket(),
              err, str_error(err));
}

}
