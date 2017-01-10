
#ifndef ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_
#define ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_

#include <assert.h>
#include <new>

#include <nut/rc/rc_new.h>

#include "proact_handler.h"
#include "../inet_base/inet_addr.h"

namespace loofah
{

class LOOFAH_API ProactAcceptorBase : public ProactHandler
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

    virtual void handle_read_completed(int cb) final override
    {
        // Dummy for an acceptor
    }

    virtual void handle_write_completed(int cb) final override
    {
        // Dummy for an acceptor
    }
};

template <typename CHANNEL>
class ProactAcceptor : public ProactAcceptorBase
{
public:
    virtual void handle_accept_completed(socket_t fd) override
    {
        assert(LOOFAH_INVALID_SOCKET_FD != fd);

        // Create new handler
        nut::rc_ptr<CHANNEL> channel = nut::rc_new<CHANNEL>();
        assert(NULL != channel);
        channel->initialize();
        channel->open(fd);
        channel->handle_connected();
    }
};

}

#endif
