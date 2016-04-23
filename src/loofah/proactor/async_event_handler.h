
#ifndef ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_
#define ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_

#include "../loofah.h"

namespace loofah
{

class AsyncEventHandler
{
public:
    enum EventType
    {
        READ_MASK = 1,
        WRITE_MASK = 1 << 1,
        EXCEPT_MASK = 1 << 2,
    };

    virtual ~AsyncEventHandler() {}

    virtual handle_t get_handle() const = 0;

    virtual void handle_read_completed() {}
    virtual void handle_write_completed() {}
    virtual void handle_exception() {}
};

}

#endif
