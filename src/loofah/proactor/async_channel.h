
#ifndef ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_
#define ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_

#include "async_event_handler.h"
#include "../base/channel.h"
#include "../base/sock_stream.h"

namespace loofah
{

class Proactor;

class LOOFAH_API AsyncChannel : public Channel, public AsyncEventHandler
{
protected:
    SockStream _sock_stream;

public:
    virtual void open(socket_t fd) override
    {
        _sock_stream.open(fd);
    }

    virtual socket_t get_socket() const
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
