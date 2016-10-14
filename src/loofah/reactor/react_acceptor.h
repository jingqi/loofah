
#ifndef ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_
#define ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_

#include <assert.h>
#include <stdlib.h>
#include <new> // for placement new

#include "react_handler.h"
#include "../base/inet_addr.h"

namespace loofah
{

class LOOFAH_API ReactAcceptorBase : public ReactHandler
{
    socket_t _listener_socket = INVALID_SOCKET_VALUE;

public:
    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool open(const INETAddr& addr, int listen_num = 2048);

    virtual socket_t get_socket() const override
    {
        return _listener_socket;
    }

    static socket_t handle_accept(socket_t listener_socket);
};

template <typename CHANNEL>
class ReactAcceptor : public ReactAcceptorBase
{
public:
    virtual void handle_read_ready() override
    {
        // Accept
        socket_t fd = handle_accept(get_socket());
        assert(INVALID_SOCKET_VALUE != fd);

        // Create new handler
        CHANNEL *handler = (CHANNEL*) ::malloc(sizeof(CHANNEL));
        assert(NULL != handler);
        new (handler) CHANNEL;
        handler->open(fd);
    }
};

}

#endif
