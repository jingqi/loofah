
#ifndef ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_
#define ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_

#include <assert.h>
#include <new>

#include "async_event_handler.h"

namespace loofah
{

class Proactor;

class AsyncAcceptorBase : public AsyncEventHandler
{
    socket_t _listen_socket = INVALID_SOCKET_VALUE;

protected:
    socket_t handle_accept(struct IOContext *io_context);

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

template <typename CHANNEL>
class AsyncAcceptor : public AsyncAcceptorBase
{
public:
    virtual void handle_accept_completed(struct IOContext *io_context) override
    {
#if NUT_PLATFORM_OS_WINDOWS
        handle_accept(io_context);

        // create handler
        socket_t fd = io_context->accept_socket;
        assert(INVALID_SOCKET_VALUE != fd);
        CHANNEL *handler = (CHANNEL*) ::malloc(sizeof(CHANNEL));
        assert(NULL != handler);
        new (handler) CHANNEL;
        handler->open(fd);
#endif
    }
};

}

#endif
