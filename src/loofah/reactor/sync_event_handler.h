
#ifndef ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_
#define ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_

#include "../loofah.h"

namespace loofah
{

class SyncEventHandler
{
public:
    enum EventType
    {
        READ_MASK = 1,
        WRITE_MASK = 1 << 1,
        EXCEPT_MASK = 1 << 2,
    };

    virtual ~SyncEventHandler() {}

    virtual handle_t get_handle() const = 0;

    virtual void handle_read_ready() {}
    virtual void handle_write_ready() {}
    virtual void handle_exception() {}
};

}

#endif
