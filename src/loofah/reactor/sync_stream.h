
#ifndef ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_
#define ___HEADFILE_C483D9A1_6AFD_4289_AC7B_07456CB8483F_

#include <stdio.h>
#include <unistd.h>

#include "sync_event_handler.h"

namespace loofah
{

class SyncStream : public SyncEventHandler
{
protected:
    handle_t _fd = INVALID_HANDLE;

public:
    virtual void open(handle_t fd);

    virtual handle_t get_handle() const override
    {
        return _fd;
    }
};

}

#endif
