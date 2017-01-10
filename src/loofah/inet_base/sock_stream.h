
#ifndef ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_
#define ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_

#include "../loofah_config.h"
#include "inet_addr.h"
#include "sock_operation.h"

namespace loofah
{

class LOOFAH_API SockStream
{
protected:
    socket_t _socket_fd = LOOFAH_INVALID_SOCKET_FD;
    bool _reading_shutdown = false, _writing_shutdown = false;

public:
    ~SockStream();

    void open(socket_t fd);

    socket_t get_socket() const
    {
        return _socket_fd;
    }

    bool is_valid() const
    {
        return LOOFAH_INVALID_SOCKET_FD != _socket_fd;
    }

    void close();

    bool shutdown_read();

    bool is_reading_shutdown() const
    {
        return _reading_shutdown;
    }

    void set_reading_shutdown()
    {
        _reading_shutdown = true;
    }

    bool shutdown_write();

    bool is_writing_shutdown() const
    {
        return _writing_shutdown;
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
        return SockOperation::read(_socket_fd, buf, max_len);
    }

    /**
     * 写(阻塞)
     */
    int write(const void *buf, unsigned max_len)
    {
        return SockOperation::write(_socket_fd, buf, max_len);
    }

    InetAddr get_local_addr() const
    {
        return SockOperation::get_local_addr(_socket_fd);
    }

    InetAddr get_peer_addr() const
    {
        return SockOperation::get_peer_addr(_socket_fd);
    }

    bool is_self_connected() const
    {
        return SockOperation::is_self_connected(_socket_fd);
    }

    bool set_tcp_nodelay(bool no_delay = true)
    {
        return SockOperation::set_tcp_nodelay(_socket_fd, no_delay);
    }
};

}

#endif
