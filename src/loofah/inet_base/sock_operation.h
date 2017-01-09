﻿
#ifndef ___HEADFILE_9092C70E_21B8_4084_AB7B_A802D642D26A_
#define ___HEADFILE_9092C70E_21B8_4084_AB7B_A802D642D26A_

#include "../loofah_config.h"

#include <nut/platform/stdint_traits.h> // for ssize_t in windows VC

#include "inet_addr.h"

namespace loofah
{

class LOOFAH_API SockOperation
{
private:
    // Invalid methods
    SockOperation();
    SockOperation(const SockOperation&);

public: // Common operations
    /**
     * 设置非阻塞
     */
    static bool set_nonblocking(socket_t socket_fd, bool nonblocking = true);

    /**
     * 程序退出时关闭连接
     */
    static bool set_close_on_exit(socket_t socket_fd, bool close_on_exit = true);

    /**
     * 关闭 socket
     */
    static void close(socket_t socket_fd);

public: // Listener socket operations
    /**
     * 让监听的端口可以被复用
     * NOTE 必须在 bind() 之前调用
     */
    static bool set_reuse_addr(socket_t listener_socket_fd);
    static bool set_reuse_port(socket_t listener_socket_fd);


public: // Socket connection operations
    static bool shutdown_read(socket_t socket_fd);
    static bool shutdown_write(socket_t socket_fd);

    /**
     * 读
     *
     * @return >0 读成功, 返回读取到的字节数
     *         =0 对方关闭了链接, 或者对方关闭了写通道, 或者己方关闭了读通道
     *         <0 出错
     */
    static ssize_t read(socket_t socket_fd, void *buf, size_t len);
    static ssize_t readv(socket_t socket_fd, void* const *buf_ptrs,
                         const size_t *len_ptrs, size_t buf_count);

    /**
     * 写
     *
     * @return >0 写成功, 返回写入的字节数
     *         <0 出错
     */
    static ssize_t write(socket_t socket_fd, const void *buf, size_t len);
    static ssize_t writev(socket_t socket_fd, const void* const *buf_ptrs,
                          const size_t *len_ptrs, size_t buf_count);

    static InetAddr get_local_addr(socket_t socket_fd);
    static InetAddr get_peer_addr(socket_t socket_fd);

    /**
     * 检测 socket 是否是自连接
     */
    static bool is_self_connected(socket_t socket_fd);

    /**
     * 参见 http://www.secbox.cn/hacker/5998.html
     */
    static bool set_tcp_nodelay(socket_t socket_fd, bool no_delay = true);

    static bool set_keep_alive(socket_t socket_fd, bool keep_alive = true);
};

}

#endif