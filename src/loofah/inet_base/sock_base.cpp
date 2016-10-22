
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#   include <mswsock.h>
#   include <io.h> // for ::read() and ::write()
#else
#   include <unistd.h> // for ::read() and ::write()
#   include <sys/socket.h> // for ::setsockopt() and so on
#   include <netinet/tcp.h> // for defination of TCP_NODELAY
#   include <fcntl.h> // for ::fcntl()
#endif

#include <nut/logging/logger.h>

#include "sock_base.h"


#define TAG "loofah.sock_base"

namespace loofah
{

bool SockBase::set_nonblocking(socket_t socket_fd, bool nonblocking)
{
#if NUT_PLATFORM_OS_WINDOWS
    unsigned long mode = (nonblocking ? 1 : 0);
    return ::ioctlsocket(socket_fd, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return ::fcntl(socket_fd, F_SETFL, flags) == 0;
#endif
}

bool SockBase::set_close_on_exit(socket_t socket_fd, bool close_on_exit)
{
#if NUT_PLATFORM_OS_WINDOWS
    // TODO
    NUT_LOG_E(TAG, "operation not implemented in Windows");
    return false;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (close_on_exit)
        flags |= O_CLOEXEC;
    else
        flags &= ~O_CLOEXEC;
    return ::fcntl(socket_fd, F_SETFL, flags) == 0;
#endif
}

void SockBase::shutdown(socket_t socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    ::closesocket(socket_fd);
#else
    ::close(socket_fd);
#endif
}

bool SockBase::set_reuse_addr(socket_t listener_socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = TRUE;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                             (char*) &optval, sizeof(optval));
#else
    int optval = 1;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                             &optval, sizeof(optval));
#endif
}

bool SockBase::set_reuse_port(socket_t listener_socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = TRUE;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEPORT,
                             (char*) &optval, sizeof(optval));
#else
    int optval = 1;
    return 0 == ::setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEPORT,
                             &optval, sizeof(optval));
#endif
}

bool SockBase::shutdown_read(socket_t socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    // TODO
    NUT_LOG_E(TAG, "operation not implemented in Windows");
    return false;
#else
    const int rs = ::shutdown(socket_fd, SHUT_RD);
    if (0 != rs)
    {
        NUT_LOG_E(TAG, "failed to call ::shutdown() with errno %d: %s",
                  errno, ::strerror(errno));
    }
    return 0 == rs;
#endif
}

bool SockBase::shutdown_write(socket_t socket_fd)
{
#if NUT_PLATFORM_OS_WINDOWS
    // TODO
    NUT_LOG_E(TAG, "operation not implemented in Windows");
    return false;
#else
    const int rs = ::shutdown(socket_fd, SHUT_WR);
    if (0 != rs)
    {
        NUT_LOG_E(TAG, "failed to call ::shutdown() with errno %d: %s",
                  errno, ::strerror(errno));
    }
    return 0 == rs;
#endif
}

int SockBase::read(socket_t socket_fd, void *buf, unsigned max_len)
{
    assert(NULL != buf || 0 == max_len);
    return ::recv(socket_fd, (char*) buf, max_len, 0);
}

int SockBase::write(socket_t socket_fd, const void *buf, unsigned max_len)
{
    assert(NULL != buf || 0 == max_len);
    return ::send(socket_fd, (const char*) buf, max_len, 0);
}

InetAddr SockBase::get_local_addr(socket_t socket_fd)
{
    InetAddr ret;
#if NUT_PLATFORM_OS_WINDOWS
    int plen = ret.get_max_sockaddr_size();
#else
    socklen_t plen = ret.get_max_sockaddr_size();
#endif
    if (::getsockname(socket_fd, ret.cast_to_sockaddr(), &plen) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::getsockname(), socketfd %d", socket_fd);
        return ret;
    }
    return ret;
}

InetAddr SockBase::get_peer_addr(socket_t socket_fd)
{
    InetAddr ret;
#if NUT_PLATFORM_OS_WINDOWS
    int plen = ret.get_max_sockaddr_size();
#else
    socklen_t plen = ret.get_max_sockaddr_size();
#endif
    if (::getpeername(socket_fd, ret.cast_to_sockaddr(), &plen) < 0)
    {
        NUT_LOG_E(TAG, "failed to call ::getpeername(), socketfd %d", socket_fd);
        return ret;
    }
    return ret;
}

bool SockBase::is_self_connected(socket_t socket_fd)
{
    return get_local_addr(socket_fd) == get_peer_addr(socket_fd);
}

bool SockBase::set_tcp_nodelay(socket_t socket_fd, bool no_delay)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = no_delay ? TRUE : FALSE;
    return 0 == ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
                             (char*) &optval, sizeof(optval));
#else
    int optval = no_delay ? 1 : 0;
    return 0 == ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
                             &optval, sizeof(optval));
#endif
}

bool SockBase::set_keep_alive(socket_t socket_fd, bool keep_alive)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = keep_alive ? TRUE : FALSE;
    return 0 == ::setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE,
                             (char*) &optval, sizeof(optval));
#else
    int optval = keep_alive ? 1 : 0;
    return 0 == ::setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE,
                             &optval, sizeof(optval));
#endif
}

}
