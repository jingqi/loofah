
#ifndef ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_
#define ___HEADFILE_037D5BEB_C395_4C0A_A957_F6A95AA9F1D8_

#include <assert.h>

#include <nut/rc/rc_new.h>

#include "../inet_base/inet_addr.h"
#include "proact_handler.h"


namespace loofah
{

class ProactChannel;

class LOOFAH_API ProactAcceptorBase : public ProactHandler
{
public:
    ~ProactAcceptorBase();

    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool listen(const InetAddr& addr, int listen_num = 2048);

    virtual socket_t get_socket() const final override;
    virtual void handle_accept_completed(socket_t fd) final override;
    virtual void handle_connect_completed() final override;
    virtual void handle_read_completed(size_t cb) final override;
    virtual void handle_write_completed(size_t cb) final override;
    virtual void handle_io_exception(int err) final override;

protected:
    virtual nut::rc_ptr<ProactChannel> create_channel() = 0;

private:
    socket_t _listening_socket = LOOFAH_INVALID_SOCKET_FD;
};

template <typename CHANNEL>
class ProactAcceptor : public ProactAcceptorBase
{
private:
    virtual nut::rc_ptr<ProactChannel> create_channel() final override
    {
        return nut::rc_new<CHANNEL>();
    }
};

}

#endif
