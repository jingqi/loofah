
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
#   include <sys/uio.h> // for ::readv()
#   include <errno.h>
#   include <string.h> // for ::strerror()
#endif

#include <nut/logging/logger.h>

#include "sock_operation.h"
#include "error.h"


#define TAG "loofah.inet_base.sock_operation"

namespace loofah
{

bool SockOperation::set_nonblocking(socket_t socket_fd, bool nonblocking) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    unsigned long mode = (nonblocking ? 1 : 0);
    const int rs = ::ioctlsocket(socket_fd, FIONBIO, &mode);
    if (0 != rs)
        LOOFAH_LOG_ERR_FD(ioctlsocket, socket_fd);
    return 0 == rs;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
    {
        LOOFAH_LOG_ERR_FD(fcntl, socket_fd);
        return false;
    }

    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    const int rs = ::fcntl(socket_fd, F_SETFL, flags);
    if (0 != rs)
        LOOFAH_LOG_ERR_FD(fcntl, socket_fd);
    return 0 == rs;
#endif
}

bool SockOperation::set_close_on_exit(socket_t socket_fd, bool close_on_exit) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    NUT_LOG_W(TAG, "SockOperation::set_close_on_exit() not implemented in Windows");
    return false;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
    {
        LOOFAH_LOG_ERR_FD(fcntl, socket_fd);
        return false;
    }

    if (close_on_exit)
        flags |= O_CLOEXEC;
    else
        flags &= ~O_CLOEXEC;
    const int rs = ::fcntl(socket_fd, F_SETFL, flags);
    if (0 != rs)
        LOOFAH_LOG_ERR_FD(fcntl, socket_fd);
    return 0 == rs;
#endif
}

void SockOperation::close(socket_t socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    if (0 != ::closesocket(socket_fd))
        LOOFAH_LOG_ERR_FD(closesocket, socket_fd);
#else
    if (0 != ::close(socket_fd))
        LOOFAH_LOG_ERR_FD(close, socket_fd);
#endif
}

bool SockOperation::is_valid(socket_t socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    DWORD socket_type = 0;
    int len = sizeof(socket_type);
    if (0 == ::getsockopt(socket_fd, SOL_SOCKET, SO_TYPE, (char*) &socket_type, &len))
        return true;
    return WSAENOTSOCK != ::WSAGetLastError();
#else
    return ::fcntl(socket_fd, F_GETFL, 0) >= 0;
#endif
}

int SockOperation::get_last_error(socket_t socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    DWORD error = 0;
    int len = sizeof(error);
    if (0 != ::getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, (char*) &error, &len))
        return ::WSAGetLastError();
    return (int) error;
#else
    int error = 0;
    socklen_t len = sizeof(error);
    if (0 != ::getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len))
        return errno;
    return error;
#endif
}

bool SockOperation::set_reuse_addr(socket_t listening_socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 在 Windows 上，设置 ReuseAddr 与在 *nix 上不同，参见
    // http://stackoverflow.com/questions/17212789/multiple-processes-listening-on-the-same-port
    BOOL optval = TRUE;
    const int rs = ::setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                                (char*) &optval, sizeof(optval));
#else
    int optval = 1;
    const int rs = ::setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                                &optval, sizeof(optval));
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(setsockopt, listening_socket_fd);
    return 0 == rs;
}

bool SockOperation::set_reuse_port(socket_t listening_socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 在 Windows 上，设置 ReuseAddr 与在 *nix 上不同，参见
    // http://stackoverflow.com/questions/17212789/multiple-processes-listening-on-the-same-port
    BOOL optval = TRUE;
    const int rs = ::setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR,
                                (char*) &optval, sizeof(optval));
#else
    int optval = 1;
    const int rs = ::setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEPORT,
                                &optval, sizeof(optval));
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(setsockopt, listening_socket_fd);
    return 0 == rs;
}

bool SockOperation::shutdown_read(socket_t socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    const int rs = ::shutdown(socket_fd, SD_RECEIVE);
#else
    const int rs = ::shutdown(socket_fd, SHUT_RD);
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(shutdown, socket_fd);
    return 0 == rs;
}

bool SockOperation::shutdown_write(socket_t socket_fd) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    const int rs = ::shutdown(socket_fd, SD_SEND);
#else
    const int rs = ::shutdown(socket_fd, SHUT_WR);
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(shutdown, socket_fd);
    return 0 == rs;
}

ssize_t SockOperation::read(socket_t socket_fd, void *buf, size_t len) noexcept
{
    assert(nullptr != buf);

    /*
     * recv() 返回:
     *   >0 读取的字节数
     *    0 读通道已经正常关闭(end-of-file)，或者传入的参数 len == 0
     *   -1 出错, errno 被设置
     *
     * errno 值:
     *   EAGAIN / EWOULDBLOCK 非阻塞 socket 读操作将会阻塞; 或者设置过读超时, 而
     *       读超时被触发
     *
     * WSAGetLastError() 返回:
     *   WSAESHUTDOWN   读通道已经关闭
     *   WSAEWOULDBLOCK 套接口被标志为非阻塞，但是操作不能立即完成
     */
    const ssize_t rs = ::recv(socket_fd, (char*) buf, len, 0);
    if (rs >= 0)
        return rs;

#if NUT_PLATFORM_OS_WINDOWS
    const int err = ::WSAGetLastError();
    if (WSAESHUTDOWN == err)
        return 0;
    else if (WSAEWOULDBLOCK == err)
        return LOOFAH_ERR_WOULD_BLOCK;
    LOOFAH_LOG_ERR_FD(recv, socket_fd);
    return from_errno(err);
#else
    if (EAGAIN == errno || EWOULDBLOCK == errno)
        return LOOFAH_ERR_WOULD_BLOCK;
    LOOFAH_LOG_ERR_FD(recv, socket_fd);
    return from_errno(errno);
#endif
}

ssize_t SockOperation::readv(socket_t socket_fd, void* const *buf_ptrs,
                             const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != buf_ptrs && nullptr != len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    WSABUF *wsabufs = (WSABUF*) ::alloca(sizeof(WSABUF) * buf_count);
    size_t total_bytes = 0;
    for (size_t i = 0; i < buf_count; ++i)
    {
        wsabufs[i].buf = (char*) *buf_ptrs;
        wsabufs[i].len = *len_ptrs;
        total_bytes += *len_ptrs;

        ++buf_ptrs;
        ++len_ptrs;
    }

    /*
     * WSARecv() 返回:
     *   非重叠(非异步)操作:
     *      >0 成功，读取到的字节数
     *       0 连接中断
     *      -1 SOCKET_ERROR, 错误
     *   重叠(异步)操作:
     *       0 成功并立即完成
     *      -1 SOCKET_ERROR, 需要查询 WSAGetLastError()
     *
     * WSAGetLastError() 返回:
     *   WSA_IO_PENDING 重叠操作成功启动，过后将有完成指示
     *   WSAESHUTDOWN   读通道已经关闭
     *   WSAEDISCON     远端优雅的结束了连接
     *   WSAEWOULDBLOCK 重叠套接口：太多重叠的输入/输出请求
     *                  非重叠套接口：套接口被标志为非阻塞，但是操作不能立即完成
     */
    DWORD bytes = 0, flags = 0;
    const int rs = ::WSARecv(socket_fd,
                             wsabufs,
                             buf_count, // wsabuf 的数量
                             &bytes, // 如果接收操作立即完成，这里会返回函数调用所接收到的字节数
                             &flags, // NOTE 貌似这里设置为 nullptr 会导致错误
                             nullptr,
                             nullptr);
    if (SOCKET_ERROR != rs)
        return bytes;

    const int err = ::WSAGetLastError();
    if (WSA_IO_PENDING == err)
        return total_bytes; // FIXME 重叠操作再次启动后也不一定保证全部完成
    else if (WSAESHUTDOWN == err || WSAEDISCON == err)
        return 0;
    else if (WSAEWOULDBLOCK == err)
        return LOOFAH_ERR_WOULD_BLOCK;

    LOOFAH_LOG_ERR_FD(WSARecv, socket_fd);
    return from_errno(err);
#else
    struct iovec *iovs = (struct iovec*) ::alloca(sizeof(struct iovec) * buf_count);
    for (size_t i = 0; i < buf_count; ++i)
    {
        iovs[i].iov_base = (char*) *buf_ptrs;
        iovs[i].iov_len = *len_ptrs;

        ++buf_ptrs;
        ++len_ptrs;
    }

    const ssize_t rs = ::readv(socket_fd, iovs, buf_count);
    if (rs >= 0)
        return rs;
    else if (EAGAIN == errno || EWOULDBLOCK == errno)
        return LOOFAH_ERR_WOULD_BLOCK;

    LOOFAH_LOG_ERR_FD(readv, socket_fd);
    return from_errno(errno);
#endif
}

ssize_t SockOperation::write(socket_t socket_fd, const void *buf, size_t len) noexcept
{
    assert(nullptr != buf);

    /*
     * recv() 返回:
     *   >=0 发送的字节数
     *    -1 出错, errno 被设置
     *
     * errno 值:
     *   EAGAIN / EWOULDBLOCK 非阻塞 socket 读操作将会阻塞
     *
     * WSAGetLastError() 返回:
     *   WSAESHUTDOWN   读通道已经关闭
     *   WSAEWOULDBLOCK 套接口被标志为非阻塞，但是操作不能立即完成
     */
    const ssize_t rs = ::send(socket_fd, (const char*) buf, len, 0);
    if (rs >= 0)
        return rs;

#if NUT_PLATFORM_OS_WINDOWS
    const int err = ::WSAGetLastError();
    if (WSAEWOULDBLOCK == err)
        return LOOFAH_ERR_WOULD_BLOCK;
    LOOFAH_LOG_ERR_FD(send, socket_fd);
    return from_errno(err);
#else
    if (EAGAIN == errno || EWOULDBLOCK == errno)
        return LOOFAH_ERR_WOULD_BLOCK;
    LOOFAH_LOG_ERR_FD(send, socket_fd);
    return from_errno(errno);
#endif
}

ssize_t SockOperation::writev(socket_t socket_fd, const void* const *buf_ptrs,
                         const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != buf_ptrs && nullptr != len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    WSABUF *wsabufs = (WSABUF*) ::alloca(sizeof(WSABUF) * buf_count);
    size_t total_bytes = 0;
    for (size_t i = 0; i < buf_count; ++i)
    {
        wsabufs[i].buf = (char*) *buf_ptrs;
        wsabufs[i].len = *len_ptrs;
        total_bytes += *len_ptrs;

        ++buf_ptrs;
        ++len_ptrs;
    }

    /*
     * WSASend() 返回:
     *    0 成功并立即完成
     *   -1 SOCKET_ERROR, 需要查询 WSAGetLastError()
     *
     * WSAGetLastError() 返回:
     *   WSA_IO_PENDING 重叠操作成功启动，过后将有完成指示
     *   WSAESHUTDOWN   套接口已经关闭
     *   WSAEWOULDBLOCK 重叠套接口：太多重叠的输入/输出请求
     *                  非重叠套接口：套接口被标志为非阻塞，但是操作不能立即完成
     */
    DWORD bytes = 0;
    const int rs = ::WSASend(socket_fd,
                             wsabufs,
                             buf_count, // wsabuf 的数量
                             &bytes, // 如果发送操作立即完成，这里会返回函数调用所发送的字节数
                             0,
                             nullptr,
                             nullptr);
    if (SOCKET_ERROR != rs)
        return bytes;

    const int err = ::WSAGetLastError();
    if (WSA_IO_PENDING == err)
        return total_bytes;
    else if (WSAESHUTDOWN == err)
        return 0;
    else if (WSAEWOULDBLOCK == err)
        return LOOFAH_ERR_WOULD_BLOCK;

    LOOFAH_LOG_ERR_FD(WSASend, socket_fd);
    return from_errno(err);
#else
    struct iovec *iovs = (struct iovec*) ::alloca(sizeof(struct iovec) * buf_count);
    for (size_t i = 0; i < buf_count; ++i)
    {
        iovs[i].iov_base = (char*) *buf_ptrs;
        iovs[i].iov_len = *len_ptrs;

        ++buf_ptrs;
        ++len_ptrs;
    }

    const ssize_t rs = ::writev(socket_fd, iovs, buf_count);
    if (rs >= 0)
        return rs;
    else if (EAGAIN == errno || EWOULDBLOCK == errno)
        return LOOFAH_ERR_WOULD_BLOCK;

    LOOFAH_LOG_ERR_FD(writev, socket_fd);
    return from_errno(errno);
#endif
}

InetAddr SockOperation::get_local_addr(socket_t socket_fd) noexcept
{
    InetAddr ret;
    socklen_t plen = ret.get_max_sockaddr_size();
    const int rs = ::getsockname(socket_fd, ret.cast_to_sockaddr(), &plen);
    if (0 != rs)
        LOOFAH_LOG_ERR_FD(getsockname, socket_fd);
    return ret;
}

InetAddr SockOperation::get_peer_addr(socket_t socket_fd) noexcept
{
    InetAddr ret;
    socklen_t plen = ret.get_max_sockaddr_size();
    const int rs = ::getpeername(socket_fd, ret.cast_to_sockaddr(), &plen);
    if (0 != rs)
        LOOFAH_LOG_ERR_FD(getpeername, socket_fd);
    return ret;
}

bool SockOperation::is_self_connected(socket_t socket_fd) noexcept
{
    return get_local_addr(socket_fd) == get_peer_addr(socket_fd);
}

bool SockOperation::set_tcp_nodelay(socket_t socket_fd, bool no_delay) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = no_delay ? TRUE : FALSE;
    const int rs = ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
                                (char*) &optval, sizeof(optval));
#else
    int optval = no_delay ? 1 : 0;
    const int rs = ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY,
                                &optval, sizeof(optval));
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(setsockopt, socket_fd);
    return 0 == rs;
}

bool SockOperation::set_keep_alive(socket_t socket_fd, bool keep_alive) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = keep_alive ? TRUE : FALSE;
    const int rs = ::setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE,
                                (char*) &optval, sizeof(optval));
#else
    int optval = keep_alive ? 1 : 0;
    const int rs = ::setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE,
                                &optval, sizeof(optval));
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(setsockopt, socket_fd);
    return 0 == rs;
}

bool SockOperation::set_linger(socket_t socket_fd, bool on, unsigned time) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    struct linger so_linger;
    so_linger.l_onoff = (on ? TRUE : FALSE);
    so_linger.l_linger = time;
    const int rs = ::setsockopt(socket_fd, SOL_SOCKET, SO_LINGER,
                                (char*) &so_linger, sizeof(so_linger));
#else
    struct linger so_linger;
    so_linger.l_onoff = (on ? 1 : 0);
    so_linger.l_linger = time;
    const int rs = ::setsockopt(socket_fd, SOL_SOCKET, SO_LINGER,
                                &so_linger, sizeof(so_linger));
#endif

    if (0 != rs)
        LOOFAH_LOG_ERR_FD(setsockopt, socket_fd);
    return 0 == rs;
}

}
