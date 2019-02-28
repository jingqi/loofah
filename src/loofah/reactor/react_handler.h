
#ifndef ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_
#define ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_

#include "../loofah_config.h"

#include <nut/rc/rc_ptr.h>


namespace loofah
{

class ReactHandler
{
    NUT_REF_COUNTABLE

public:
    enum EventType
    {
        READ_MASK = 1,
        WRITE_MASK = 1 << 1,
        EXCEPT_MASK = 1 << 2,
    };

public:
    ReactHandler() = default;

    virtual ~ReactHandler() = default;

    virtual socket_t get_socket() const = 0;

    /**
     * acceptor 接受链接, channel 接收数据
     */
    virtual void handle_read_ready() = 0;

    /**
     * channel 发送数据
     */
    virtual void handle_write_ready() = 0;

private:
    ReactHandler(const ReactHandler&) = delete;
    ReactHandler& operator=(const ReactHandler&) = delete;

private:
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    // 用于记录注册状态，参见 Reactor 的实现
    int _registered_events = 0;
    friend class Reactor;
#endif
};

}

#endif
