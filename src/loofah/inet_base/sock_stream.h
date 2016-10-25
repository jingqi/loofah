
#ifndef ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_
#define ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_

#include "../loofah_config.h"
#include "inet_addr.h"
#include "sock_base.h"

namespace loofah
{

class LOOFAH_API SockStream
{
protected:
    socket_t _socket_fd = INVALID_SOCKET_VALUE;

public:
    socket_t get_socket() const
    {
        return _socket_fd;
    }

    void open(socket_t fd);

    void shutdown()
    {
        SockBase::shutdown(_socket_fd);
        _socket_fd = INVALID_SOCKET_VALUE;
    }

    bool shutdown_read()
    {
        return SockBase::shutdown_read(_socket_fd);
    }

    bool shutdown_write()
    {
        return SockBase::shutdown_write(_socket_fd);
    }

    /**
     * 读(阻塞)
     *
     * @return >0 读成功
     *         =0 已经关闭
     *         <0 出错
     */
    int read(void *buf, unsigned max_len)
    {
        return SockBase::read(_socket_fd, buf, max_len);
    }

    /**
     * 写(阻塞)
     */
    int write(const void *buf, unsigned max_len)
    {
        return SockBase::write(_socket_fd, buf, max_len);
    }

    InetAddr get_local_addr() const
    {
        return SockBase::get_local_addr(_socket_fd);
    }

    InetAddr get_peer_addr() const
    {
        return SockBase::get_peer_addr(_socket_fd);
    }

    bool is_self_connected() const
    {
        return SockBase::is_self_connected(_socket_fd);
    }

    bool set_tcp_nodelay(bool no_delay = true)
    {
        return SockBase::set_tcp_nodelay(_socket_fd, no_delay);
    }
};

}

#endif
