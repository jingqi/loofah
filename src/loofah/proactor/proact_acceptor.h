
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

public:
    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool open(const INETAddr& addr, int listen_num = 2048);

    virtual socket_t get_socket() const override
    {
        return _listen_socket;
    }

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    static socket_t handle_accept(socket_t listener_sock_fd);
#endif
};

template <typename CHANNEL>
class ProactAcceptor : public ProactAcceptorBase
{
public:
    virtual void handle_accept_completed(socket_t fd) override
    {
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
