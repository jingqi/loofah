
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <sys/socket.h> // for ::socket() and so on
#   include <netinet/in.h> // for sockaddr_in
#   include <unistd.h> // for ::close()
#   include <errno.h>
#   include <string.h> // for ::strerror()
#endif

#include <nut/logging/logger.h>

#include "react_acceptor.h"
#include "../base/utils.h"
#include "../base/sock_base.h"

#define TAG "loofah.react_acceptor"

namespace loofah
{

bool ReactAcceptorBase::open(const INETAddr& addr, int listen_num)
{
    // Create socket
    _listener_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == _listener_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // Make port reuseable
    if (!SockBase::make_listen_addr_reuseable(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket reuseable, socketfd %d", _listener_socket);

    // Bind
    const struct sockaddr_in& sin = addr.get_sockaddr_in();
    // sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(INADDR_ANY);
    // sin.sin_port = htons(port);
    if (::bind(_listener_socket, (const struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listener_socket);
#else
        ::close(_listener_socket);
#endif
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (::listen(_listener_socket, listen_num) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listener_socket);
#else
        ::close(_listener_socket);
#endif
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Make socket non-blocking
    if (!SockBase::make_nonblocking(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listener_socket);

    return true;
}

socket_t ReactAcceptorBase::handle_accept(socket_t listener_socket)
{
    struct sockaddr_in remote_addr;
#if NUT_PLATFORM_OS_WINDOWS
    int rsz = sizeof(remote_addr);
#else
    socklen_t rsz = sizeof(remote_addr);
#endif
    socket_t fd = ::accept(listener_socket, (struct sockaddr*)&remote_addr, &rsz);
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

    if (!SockBase::make_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}

}
