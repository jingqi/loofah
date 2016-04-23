
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
    handle_t _listen_socket_fd = INVALID_HANDLE;

protected:
    handle_t handle_accept();

public:
    bool open(int port, int listen_num = 10);

    virtual handle_t get_handle() const override
    {
        return _listen_socket_fd;
    }
};

template <typename STREAM>
class SyncAcceptor : public SyncAcceptorBase
{
public:
    virtual void handle_read_ready() override
    {
        handle_t fd = handle_accept();
        STREAM *handler = (STREAM*) ::malloc(sizeof(STREAM));
        assert(NULL != handler);
        new (handler) STREAM;
        handler->open(fd);
    }
};

}

#endif
