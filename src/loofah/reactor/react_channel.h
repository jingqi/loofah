﻿
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

public:
    virtual void open(socket_t fd) override;

    virtual socket_t get_socket() const override;

    SockStream& get_sock_stream();

protected:
    SockStream _sock_stream;
};

}

#endif
