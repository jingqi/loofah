
#include <assert.h>
#include <new>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <unistd.h> // for ::close()
#endif

#include <nut/logging/logger.h>

#include "proact_acceptor.h"
#include "../base/utils.h"

#define TAG "loofah.proact_acceptor"

namespace loofah
{

bool ProactAcceptorBase::open(const INETAddr& addr, int listen_num)
{
#if NUT_PLATFORM_OS_WINDOWS
    // Create socket
    // NOTE 必须使用 ::WSASocket() 创建 socket 并带上 WSA_FLAG_OVERLAPPED 标记，
    //      以便使用完成端口
    _listen_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET_VALUE == _listen_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::WSASocket()");
        return false;
    }

    // Bind
    const struct sockaddr_in& sin = addr.get_sockaddr_in();
    // sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(INADDR_ANY);
    // sin.sin_port = htons(port);
    if (SOCKET_ERROR == ::bind(_listen_socket, (const struct sockaddr*)&sin, sizeof(sin)))
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (SOCKET_ERROR == ::listen(_listen_socket, listen_num))
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    return true;
#else
    // Create socket
    _listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == _listen_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // Make port reuseable
    if (!make_listen_socket_reuseable(_listen_socket))
        NUT_LOG_W(TAG, "failed to make listen socket reuseable, socketfd %d", _listen_socket);

    // Bind
    const struct sockaddr_in& sin = addr.get_sockaddr_in();
    // sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(INADDR_ANY);
    // sin.sin_port = htons(port);
    if (::bind(_listen_socket, (const struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        ::close(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (::listen(_listen_socket, listen_num) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
        ::close(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Make socket non-blocking
    if (!make_socket_nonblocking(_listen_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listen_socket);

    return true;
#endif
}

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
socket_t ProactAcceptorBase::handle_accept(socket_t listener_sock_fd)
{
    struct sockaddr_in remote_addr;
    socklen_t rsz = sizeof(remote_addr);
    socket_t fd = ::accept(listener_sock_fd, (struct sockaddr*)&remote_addr, &rsz);
    if (INVALID_SOCKET_VALUE == fd)
    {
        NUT_LOG_E(TAG, "failed to call ::accept()");
        return INVALID_SOCKET_VALUE;
    }

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    if (::getpeername(fd, (struct sockaddr*)&peer, &plen) < 0)
    {
        NUT_LOG_W(TAG, "failed to call ::getpeername(), socketfd %d", fd);
        ::close(fd);
        return INVALID_SOCKET_VALUE;
    }

    if (!make_socket_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}
#endif

}
