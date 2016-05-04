
#ifndef ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_
#define ___HEADFILE_9B5119C9_43AA_4CDE_A44F_60BF923E0400_

#include "async_event_handler.h"

namespace loofah
{

class Proactor;

class AsyncStream : public AsyncEventHandler
{
protected:
    socket_t _fd = INVALID_SOCKET_VALUE;
    Proactor *_proactor = NULL;

public:
    virtual void open(socket_t fd, Proactor *proactor);

    virtual socket_t get_socket() const override
    {
        return _fd;
    }

    void launch_read();
    void launch_write();
};

}

#endif
