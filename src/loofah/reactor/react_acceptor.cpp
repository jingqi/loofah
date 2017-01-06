
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <sys/socket.h> // for ::socket() and so on
#   include <netinet/in.h> // for sockaddr_in
#   include <errno.h>
#   include <string.h> // for ::strerror()
#endif

#include <nut/logging/logger.h>

#include "react_acceptor.h"
#include "../inet_base/utils.h"
#include "../inet_base/sock_base.h"

#define TAG "loofah.react_acceptor"

namespace loofah
{

bool ReactAcceptorBase::open(const InetAddr& addr, int listen_num)
{
    // Create socket
    const int domain = addr.is_ipv6() ? AF_INET6 : AF_INET;
    _listener_socket = ::socket(domain, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == _listener_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // Make port reuseable
    if (!SockBase::set_reuse_addr(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket addr reuseable, socketfd %d", _listener_socket);
    if (!SockBase::set_reuse_port(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket port reuseable, socketfd %d", _listener_socket);

    // Bind
    if (::bind(_listener_socket, addr.cast_to_sockaddr(), addr.get_sockaddr_size()) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        SockBase::shutdown(_listener_socket);
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (::listen(_listener_socket, listen_num) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
        SockBase::shutdown(_listener_socket);
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Make socket non-blocking
    if (!SockBase::set_nonblocking(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listener_socket);

    return true;
}

socket_t ReactAcceptorBase::handle_accept(socket_t listener_socket)
{
    InetAddr peer_addr;
#if NUT_PLATFORM_OS_WINDOWS
    int rsz = peer_addr.get_max_sockaddr_size();
#else
    socklen_t rsz = peer_addr.get_max_sockaddr_size();
#endif
    socket_t fd = ::accept(listener_socket, peer_addr.cast_to_sockaddr(), &rsz);
    if (INVALID_SOCKET_VALUE == fd)
    {
#if NUT_PLATFORM_OS_WINDOWS
        const int errcode = ::WSAGetLastError();
        // NOTE 错误码 WSAEWOULDBLOCK 表示已经没有资源了，等待下次异步通知，是正常的
        if (WSAEWOULDBLOCK != errcode)
            NUT_LOG_E(TAG, "failed to call ::accept() with errno %d", errcode);
#else
        // NOTE 错误码 EAGAIN 表示已经没有资源了，等待下次异步通知，是正常的
        if (EAGAIN != errno)
        {
            NUT_LOG_E(TAG, "failed to call ::accept() with errno %d: %s", errno,
                      ::strerror(errno));
        }
#endif
        return INVALID_SOCKET_VALUE;
    }

    if (!SockBase::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}

}
