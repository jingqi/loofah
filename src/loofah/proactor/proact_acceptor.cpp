
#include "../loofah_config.h"

#include <assert.h>

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "../inet_base/utils.h"
#include "../inet_base/sock_operation.h"
#include "../inet_base/error.h"
#include "proact_acceptor.h"
#include "proact_channel.h"
#include "proactor.h"


#define TAG "loofah.proact_acceptor"

namespace loofah
{

ProactAcceptorBase::~ProactAcceptorBase() noexcept
{
    if (LOOFAH_INVALID_SOCKET_FD != _listening_socket)
        SockOperation::close(_listening_socket);
    _listening_socket = LOOFAH_INVALID_SOCKET_FD;
}

bool ProactAcceptorBase::listen(const InetAddr& addr, int listen_num) noexcept
{
    // Create socket
    const int domain = addr.is_ipv6() ? AF_INET6 : AF_INET;
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 必须使用 ::WSASocket() 创建 socket, 并带上 WSA_FLAG_OVERLAPPED 标记，
    //      以便用于 iocp
    _listening_socket = ::WSASocket(domain, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == _listening_socket)
    {
        LOOFAH_LOG_ERRNO(WSASocket);
        return false;
    }
#else
    _listening_socket = ::socket(domain, SOCK_STREAM, 0);
    if (-1 == _listening_socket)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }
#endif

    // Make port reuseable
    if (!SockOperation::set_reuse_addr(_listening_socket))
        NUT_LOG_W(TAG, "failed to make listen socket addr reuseable, socketfd %d", _listening_socket);
    if (!SockOperation::set_reuse_port(_listening_socket))
        NUT_LOG_W(TAG, "failed to make listen socket port reuseable, socketfd %d", _listening_socket);

    // Bind
    int rs = ::bind(_listening_socket, addr.cast_to_sockaddr(), addr.get_sockaddr_size());
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == rs)
#else
    if (rs < 0)
#endif
    {
        LOOFAH_LOG_FD_ERRNO(bind, _listening_socket);
        NUT_LOG_E(TAG, "failed to call bind() with addr %s", addr.to_string().c_str());
        SockOperation::close(_listening_socket);
        _listening_socket = LOOFAH_INVALID_SOCKET_FD;
        return false;
    }

    // Listen
    rs = ::listen(_listening_socket, listen_num);
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == rs)
#else
    if (rs < 0)
#endif
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

socket_t ProactAcceptorBase::get_socket() const noexcept
{
    return _listening_socket;
}

void ProactAcceptorBase::handle_accept_completed(socket_t fd) noexcept
{
    nut::rc_ptr<ProactChannel> channel = create_channel();
    channel->initialize();
    channel->open(fd);
    channel->handle_channel_connected();

    _registered_proactor->launch_accept(this);
}

void ProactAcceptorBase::handle_connect_completed() noexcept
{
    assert(false); // Should not run into this place
}

void ProactAcceptorBase::handle_read_completed(size_t cb) noexcept
{
    UNUSED(cb);
    assert(false); // Should not run into this place
}

void ProactAcceptorBase::handle_write_completed(size_t cb) noexcept
{
    UNUSED(cb);
    assert(false); // Should not run into this place
}

void ProactAcceptorBase::handle_io_error(int err) noexcept
{
    NUT_LOG_E(TAG, "fd %d, loofah error raised %d: %s", get_socket(),
              err, str_error(err));
}

}
