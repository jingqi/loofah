
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
     * @return >0 读成功，返回读取到的字节数
     *         =0 对方关闭了链接, 或者对方关闭了写通道, 或者己方关闭了读通道
     *         <0 出错
     */
    ssize_t read(void *buf, size_t max_len);

    /**
     * 写(阻塞)
     *
     * @return >=0 写成功, 返回写入的字节数
     *         <0 出错
     */
    ssize_t write(const void *buf, size_t max_len);

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
