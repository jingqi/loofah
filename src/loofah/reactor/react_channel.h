
#ifndef ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_
#define ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_

#include "../inet_base/channel.h"
#include "../inet_base/sock_stream.h"
#include "react_handler.h"


namespace loofah
{

class LOOFAH_API ReactChannel : public Channel, public ReactHandler
{
    NUT_REF_COUNTABLE_OVERRIDE

public:
    SockStream& get_sock_stream() noexcept;

    virtual void open(socket_t fd) noexcept override;

    virtual socket_t get_socket() const noexcept final override;
    virtual void handle_accept_ready() noexcept final override;
    virtual void handle_connect_ready() noexcept final override;

protected:
    SockStream _sock_stream;
};

}

#endif
