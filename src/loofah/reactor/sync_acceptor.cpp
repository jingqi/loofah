
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#else
#   include <sys/socket.h> // for socket() and so on
#   include <netinet/in.h> // for sockaddr_in
#   include <unistd.h> // for close()
#endif

#include <nut/logging/logger.h>

#include "sync_acceptor.h"
#include "../utils.h"

#define TAG "loofah.sync_acceptor"

namespace loofah
{

bool SyncAcceptorBase::open(int port, int listen_num)
{
    // new socket
    _listen_socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == _listen_socket_fd)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // make port reuseable
    if (!make_listen_socket_reuseable(_listen_socket_fd))
		NUT_LOG_W(TAG, "failed to make listen socket reuseable, socketfd %d", _listen_socket_fd);

    // bind
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);
    if (::bind(_listen_socket_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with port %d", port);
		::close(_listen_socket_fd);
		_listen_socket_fd = INVALID_HANDLE;
        return false;
    }

    // listen
    if (::listen(_listen_socket_fd, listen_num) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with port %d", port);
		::close(_listen_socket_fd);
		_listen_socket_fd = INVALID_HANDLE;
        return false;
    }

    // make socket non-blocking
    if (!make_socket_nonblocking(_listen_socket_fd))
		NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listen_socket_fd);

    return true;
}

handle_t SyncAcceptorBase::handle_accept()
{
    struct sockaddr_in remote_addr;
    socklen_t rsz = sizeof(remote_addr);
    handle_t fd = ::accept(_listen_socket_fd, (struct sockaddr*)&remote_addr, &rsz);
    if (fd < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::accept()");
        return INVALID_HANDLE;
    }

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    if (::getpeername(fd, (struct sockaddr*)&peer, &plen) < 0)
    {
        NUT_LOG_W(TAG, "failed to call ::getpeername(), socketfd %d", fd);
		::close(fd);
        return INVALID_HANDLE;
    }

    if (!make_socket_nonblocking(fd))
		NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    return fd;
}

}
