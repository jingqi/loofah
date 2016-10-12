
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <sys/socket.h> // for socket() and so on
#   include <netinet/in.h> // for sockaddr_in
#   include <unistd.h> // for close()
#endif

#include <nut/logging/logger.h>

#include "react_acceptor.h"
#include "../base/utils.h"

#define TAG "loofah.react_acceptor"

namespace loofah
{

bool ReactAcceptorBase::open(const INETAddr& addr, int listen_num)
{
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
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listen_socket);
#else
        ::close(_listen_socket);
#endif
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (::listen(_listen_socket, listen_num) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listen_socket);
#else
        ::close(_listen_socket);
#endif
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Make socket non-blocking
    if (!make_socket_nonblocking(_listen_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listen_socket);

    return true;
}

socket_t ReactAcceptorBase::handle_accept()
{
    struct sockaddr_in remote_addr;
#if NUT_PLATFORM_OS_WINDOWS
    int rsz = sizeof(remote_addr);
#else
    socklen_t rsz = sizeof(remote_addr);
#endif
    socket_t fd = ::accept(_listen_socket, (struct sockaddr*)&remote_addr, &rsz);
    if (INVALID_SOCKET_VALUE == fd)
    {
        NUT_LOG_E(TAG, "failed to call ::accept()");
        return INVALID_SOCKET_VALUE;
    }

    struct sockaddr_in peer;
#if NUT_PLATFORM_OS_WINDOWS
    int plen = sizeof(peer);
#else
    socklen_t plen = sizeof(peer);
#endif
    if (::getpeername(fd, (struct sockaddr*)&peer, &plen) < 0)
    {
        NUT_LOG_W(TAG, "failed to call ::getpeername(), socketfd %d", fd);
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(fd);
#else
        ::close(fd);
#endif
        return INVALID_SOCKET_VALUE;
    }

    if (!make_socket_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}

}
