
#ifndef ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_
#define ___HEADFILE_C5256A05_7E87_4E2F_A0B3_DBF4B7DD4015_

#include "../loofah.h"
#include "inet_addr.h"

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
    void close();

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

    INETAddr get_peer_addr() const;
};

}

#endif
