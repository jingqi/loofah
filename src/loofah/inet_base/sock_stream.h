﻿
#ifndef ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_
#define ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_

#include "../loofah_config.h"
#include "inet_addr.h"
#include "sock_operation.h"


namespace loofah
{

class LOOFAH_API SockStream
{
public:
    SockStream() = default;
    ~SockStream();

    void open(socket_t fd);
    void close();

    socket_t get_socket() const;

    bool is_valid() const;

    bool shutdown_read();
    bool is_reading_shutdown() const;
    void set_reading_shutdown();

    bool shutdown_write();
    bool is_writing_shutdown() const;

    /**
     * 读(阻塞)
     *
     * @return >0 读成功
     *         =0 已经关闭
     *         <0 出错
     */
    int read(void *buf, unsigned max_len);

    /**
     * 写(阻塞)
     */
    int write(const void *buf, unsigned max_len);

    InetAddr get_local_addr() const;
    InetAddr get_peer_addr() const;

    bool is_self_connected() const;

    bool set_tcp_nodelay(bool no_delay = true);

private:
    SockStream(const SockStream&) = delete;
    SockStream& operator=(const SockStream&) = delete;

protected:
    socket_t _socket_fd = LOOFAH_INVALID_SOCKET_FD;
    bool _reading_shutdown = false, _writing_shutdown = false;
};

}

#endif
