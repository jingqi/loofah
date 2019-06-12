
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
    ~SockStream() noexcept;

    void open(socket_t fd) noexcept;
    void close() noexcept;

    socket_t get_socket() const noexcept;

    bool is_null() const noexcept;
    bool is_valid() const noexcept;

    int get_last_error() const noexcept;

    bool shutdown_read() noexcept;
    bool is_reading_shutdown() const noexcept;
    void mark_reading_shutdown() noexcept;

    bool shutdown_write() noexcept;
    bool is_writing_shutdown() const noexcept;
    void mark_writing_shutdown() noexcept;

    /**
     * 读
     *
     * @return
     *  >0  读成功, 返回读取到的字节数(Windows 下 readv() 返回值不可靠)
     *
     *  0   EOF, 读通道已经正常关闭; 或者传入的 len==0
     *
     *  LOOFAH_ERR_UNKNOWN
     *      未知错误
     *
     *  LOOFAH_ERR_WOULD_BLOCK
     *      非阻塞 socket 读操作将会阻塞; 或者设置过读超时, 而读超时被触发
     */
    ssize_t read(void *buf, size_t max_len) noexcept;
    ssize_t readv(void* const *buf_ptrs, const size_t *len_ptrs, size_t buf_count) noexcept;

    /**
     * 写
     *
     * @return
     *  >=0 写成功, 返回写入的字节数(Windows 下 writev() 返回值不可靠)
     *
     *  LOOFAH_ERR_UNKNOWN
     *      未知错误
     *
     *  LOOFAH_ERR_WOULD_BLOCK
     *      非阻塞 socket 写操作将会阻塞
     */
    ssize_t write(const void *buf, size_t max_len) noexcept;
    ssize_t writev(const void* const *buf_ptrs, const size_t *len_ptrs, size_t buf_count) noexcept;

    InetAddr get_local_addr() const noexcept;
    InetAddr get_peer_addr() const noexcept;

    bool is_self_connected() const noexcept;

    bool set_tcp_nodelay(bool no_delay = true) noexcept;

private:
    SockStream(const SockStream&) = delete;
    SockStream& operator=(const SockStream&) = delete;

protected:
    socket_t _socket_fd = LOOFAH_INVALID_SOCKET_FD;
    bool _reading_shutdown = false, _writing_shutdown = false;
};

}

#endif
