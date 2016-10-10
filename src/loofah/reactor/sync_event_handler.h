
#ifndef ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_
#define ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_

#include "../loofah.h"

namespace loofah
{

class SyncEventHandler
{
	// 用于记录注册状态，参见 Reactor 的实现
	int _registered_events = 0;
	friend class Reactor;

public:
    enum EventType
    {
        READ_MASK = 0x01,
        WRITE_MASK = 0x02,
        EXCEPT_MASK = 0x04,
    };

    virtual ~SyncEventHandler() {}

    virtual socket_t get_socket() const = 0;

    virtual void handle_read_ready() {}
    virtual void handle_write_ready() {}
    virtual void handle_exception() {}
    virtual void handle_close() {}
};

}

#endif
