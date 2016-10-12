
#ifndef ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_
#define ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_

#include <assert.h>
#include <new>

#include "proact_handler.h"
#include "../base/inet_addr.h"

namespace loofah
{

class LOOFAH_API ProactAcceptorBase : public ProactHandler
{
    socket_t _listen_socket = INVALID_SOCKET_VALUE;

protected:
    socket_t handle_accept(IOContext *io_context);

public:
    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool open(const INETAddr& addr, int listen_num = 2048);

    virtual socket_t get_socket() const override
    {
        return _listen_socket;
    }
};

template <typename CHANNEL>
class ProactAcceptor : public ProactAcceptorBase
{
public:
    virtual void handle_accept_completed(IOContext *io_context) override
    {
#if NUT_PLATFORM_OS_WINDOWS
        // Accept
        const socket_t fd = handle_accept(io_context);
        assert(INVALID_SOCKET_VALUE != fd);

        // Create new handler
        CHANNEL *handler = (CHANNEL*) ::malloc(sizeof(CHANNEL));
        assert(NULL != handler);
        new (handler) CHANNEL;
        handler->open(fd);
#endif
    }
};

}

#endif
