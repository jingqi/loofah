﻿
#ifndef ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_
#define ___HEADFILE_42991067_03A8_4F2D_ACB6_10A384BA4ECF_

#include <assert.h>

#include <nut/rc/rc_new.h>

#include "../inet_base/inet_addr.h"
#include "react_handler.h"


namespace loofah
{

class ReactChannel;

class LOOFAH_API ReactAcceptorBase : public ReactHandler
{
public:
    ~ReactAcceptorBase() noexcept;

    /**
     * @param listen_num 在 windows 下, 可以使用 'SOMAXCONN' 表示最大允许链接数
     */
    bool listen(const InetAddr& addr, int listen_num = 2048) noexcept;

    virtual socket_t get_socket() const noexcept final override;
    virtual void handle_accept_ready() noexcept final override;
    virtual void handle_connect_ready() noexcept final override;
    virtual void handle_read_ready() noexcept final override;
    virtual void handle_write_ready() noexcept final override;
    virtual void handle_io_error(int err) noexcept final override;

    static socket_t accept(socket_t listening_socket) noexcept;

protected:
    virtual nut::rc_ptr<ReactChannel> create_channel() noexcept = 0;

private:
    socket_t _listening_socket = LOOFAH_INVALID_SOCKET_FD;
};

template <typename CHANNEL>
class ReactAcceptor : public ReactAcceptorBase
{
private:
    virtual nut::rc_ptr<ReactChannel> create_channel() noexcept final override
    {
        return nut::rc_new<CHANNEL>();
    }
};

}

#endif
