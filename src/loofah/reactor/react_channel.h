
#ifndef ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_
#define ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_

#include "react_handler.h"
#include "../inet_base/channel.h"
#include "../inet_base/sock_stream.h"

namespace loofah
{

class LOOFAH_API ReactChannel : public Channel, public ReactHandler
{
    NUT_REF_COUNTABLE_OVERRIDE

protected:
    SockStream _sock_stream;

public:
    virtual void open(socket_t fd) override
    {
        _sock_stream.open(fd);
    }

    virtual socket_t get_socket() const override
    {
        return _sock_stream.get_socket();
    }

    SockStream& get_sock_stream()
    {
        return _sock_stream;
    }
};

}

#endif
