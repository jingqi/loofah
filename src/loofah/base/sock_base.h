
#ifndef ___HEADFILE_9092C70E_21B8_4084_AB7B_A802D642D26A_
#define ___HEADFILE_9092C70E_21B8_4084_AB7B_A802D642D26A_

#include "../loofah.h"

namespace loofah
{

class LOOFAH_API SockBase
{
public:
    /**
     * 让监听的端口可以被复用
     * NOTE 必须在 bind() 之前调用
     */
    static bool make_listen_addr_reuseable(socket_t listener_socket_fd);
    
    /**
     * 设置非阻塞
     */
    static bool make_nonblocking(socket_t socket_fd, bool nonblocking = true);
};

}

#endif
