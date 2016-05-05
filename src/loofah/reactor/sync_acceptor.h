
#ifndef ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_
#define ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_

#include <assert.h>
#include <stdlib.h>
#include <new> // for placement new

#include "sync_event_handler.h"

namespace loofah
{

class SyncAcceptorBase : public SyncEventHandler
{
    socket_t _listen_socket = INVALID_SOCKET_VALUE;

protected:
    socket_t handle_accept();

public:
    /**
     * @param listen_num For windows, you can pass 'SOMAXCONN' to handle max number of sockets
     */
    bool open(int port, int listen_num = 2048);

    virtual socket_t get_socket() const override
    {
        return _listen_socket;
    }
};

template <typename STREAM>
class SyncAcceptor : public SyncAcceptorBase
{
public:
    virtual void handle_read_ready() override
    {
        // create new handler
        socket_t fd = handle_accept();
        STREAM *handler = (STREAM*) ::malloc(sizeof(STREAM));
        assert(NULL != handler);
        new (handler) STREAM;
        handler->open(fd);
    }
};

}

#endif
