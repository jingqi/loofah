
#ifndef ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_
#define ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_

#include <assert.h>
#include <stdlib.h>
#include <new> // for placement new

#include <nut/rc/rc_new.h>

#include "react_handler.h"
#include "../inet_base/inet_addr.h"

namespace loofah
{

class LOOFAH_API ReactAcceptorBase : public ReactHandler
{
    socket_t _listener_socket = LOOFAH_INVALID_SOCKET_FD;

public:
    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool open(const InetAddr& addr, int listen_num = 2048);

    virtual socket_t get_socket() const override
    {
        return _listener_socket;
    }

    static socket_t handle_accept(socket_t listener_socket);

    virtual void handle_write_ready() final override
    {
        // Dummy for an acceptor
    }
};

template <typename CHANNEL>
class ReactAcceptor : public ReactAcceptorBase
{
public:
    virtual void handle_read_ready() override
    {
        // NOTE 在 edge-trigger 模式下，需要一次接收干净
        while (true)
        {
            // Accept
            socket_t fd = handle_accept(get_socket());
            if (LOOFAH_INVALID_SOCKET_FD == fd)
                break;

            // Create new handler
            nut::rc_ptr<CHANNEL> channel = nut::rc_new<CHANNEL>();
            assert(NULL != channel);
            channel->initialize();
            channel->open(fd);
            channel->handle_connected();
        }
    }
};

}

#endif
