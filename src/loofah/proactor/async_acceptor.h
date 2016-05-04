
#ifndef ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_
#define ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_

#include <assert.h>
#include <new>

#include "async_event_handler.h"
#include "../utils.h"

namespace loofah
{

class Proactor;

class AsyncAcceptorBase : public AsyncEventHandler
{
    socket_t _listen_socket = INVALID_SOCKET_VALUE;
    Proactor *_proactor = NULL;

protected:
    socket_t handle_accept();

public:
    bool open(int port, int listen_num = SOMAXCONN);

    virtual socket_t get_socket() const override
    {
        return _listen_socket;
    }

    void associate_proactor(Proactor *proactor);

    void launch_accept();
};

template <typename STREAM>
class AsyncAcceptor : public AsyncAcceptorBase
{
public:
    virtual void handle_accept_completed(struct IOContext *io_context) override
    {
#if NUT_PLATFORM_OS_WINDOWS
        SOCKADDR_IN *remote_addr = NULL, *local_addr = NULL;
        int remote_len = sizeof(SOCKADDR_IN), local_len = sizeof(SOCKADDR_IN);
        assert(NULL != func_GetAcceptExSockaddrs);
        func_GetAcceptExSockaddrs(io_context->buf,
                                  io_context->buf_len - 2 * (sizeof(struct sockaddr_in) + 16),
                                  sizeof(struct sockaddr_in) + 16,
                                  sizeof(struct sockaddr_in) + 16,
                                  (LPSOCKADDR*)&local_addr,
                                  &local_len,
                                  (LPSOCKADDR*)&remote_addr,
                                  &remote_len);

        socket_t fd = io_context->accept_socket;
        assert(INVALID_SOCKET_VALUE != fd);
        STREAM *handler = (STREAM*) ::malloc(sizeof(STREAM));
        assert(NULL != handler);
        new (handler) STREAM;
        handler->open(fd, _proactor);
#endif
    }
};

}

#endif
