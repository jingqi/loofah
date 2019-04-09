
#ifndef ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_
#define ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_

#include "../inet_base/channel.h"
#include "../inet_base/sock_stream.h"
#include "proact_handler.h"


namespace loofah
{

class LOOFAH_API ProactChannel : public Channel, public ProactHandler
{
    NUT_REF_COUNTABLE_OVERRIDE

public:
    SockStream& get_sock_stream();

    virtual void open(socket_t fd) override;

    virtual socket_t get_socket() const final override;
    virtual void handle_accept_completed(socket_t fd) final override;
    virtual void handle_connect_completed() final override;

protected:
    SockStream _sock_stream;
};

}

#endif
