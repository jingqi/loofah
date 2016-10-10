
#ifndef ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_
#define ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_

#include <stdio.h>
#include <unistd.h>

#include "sync_event_handler.h"
#include "../inet_addr.h"

namespace loofah
{

class SyncStream : public SyncEventHandler
{
protected:
    socket_t _fd = INVALID_SOCKET_VALUE;

public:
    virtual void open(socket_t fd);

    virtual socket_t get_socket() const override
    {
        return _fd;
    }

    void close();

    INETAddr get_peer_addr() const;
};

}

#endif
