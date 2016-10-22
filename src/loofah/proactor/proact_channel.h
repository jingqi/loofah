
#ifndef ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_
#define ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_

#include "proact_handler.h"
#include "../inet_base/channel.h"
#include "../inet_base/sock_stream.h"

namespace loofah
{

class LOOFAH_API ProactChannel : public Channel, public ProactHandler
{
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
