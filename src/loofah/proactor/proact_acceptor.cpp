
#include <assert.h>
#include <new>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include <nut/logging/logger.h>

#include "proact_acceptor.h"
#include "../inet_base/utils.h"
#include "../inet_base/sock_base.h"


#define TAG "loofah.proact_acceptor"

namespace loofah
{

bool ProactAcceptorBase::open(const InetAddr& addr, int listen_num)
{
    // Create socket
    const int domain = addr.is_ipv6() ? AF_INET6 : AF_INET;
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 必须使用 ::WSASocket() 创建 socket, 并带上 WSA_FLAG_OVERLAPPED 标记，
    //      以便用于 iocp
    _listener_socket = ::WSASocket(domain, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
    _listener_socket = ::socket(domain, SOCK_STREAM, 0);
#endif
    if (INVALID_SOCKET_VALUE == _listener_socket)
    {
#if NUT_PLATFORM_OS_WINDOWS
        NUT_LOG_E(TAG, "failed to call ::WSASocket()");
#else
        NUT_LOG_E(TAG, "failed to call ::socket()");
#endif
        return false;
    }

    // Make port reuseable
    if (!SockBase::set_reuse_addr(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket addr reuseable, socketfd %d", _listener_socket);
    if (!SockBase::set_reuse_port(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket port reuseable, socketfd %d", _listener_socket);

    // Bind
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == ::bind(_listener_socket, addr.cast_to_sockaddr(), addr.get_sockaddr_size()))
#else
    if (::bind(_listener_socket, addr.cast_to_sockaddr(), addr.get_sockaddr_size()) < 0)
#endif
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        SockBase::shutdown(_listener_socket);
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == ::listen(_listener_socket, listen_num))
#else
    if (::listen(_listener_socket, listen_num) < 0)
#endif
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

}
