
#ifndef ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_
#define ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_

#include "../loofah_config.h"

#include <nut/rc/rc_ptr.h>


namespace loofah
{

class ReactHandler
{
    NUT_REF_COUNTABLE

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    friend class Reactor;
#endif

public:
    enum EventType
    {
        READ_MASK = 0x01,
        WRITE_MASK = 0x02,
        EXCEPT_MASK = 0x04,
    };

public:
    ReactHandler() = default;

    virtual ~ReactHandler() = default;

    virtual socket_t get_socket() const = 0;

    /**
     * acceptor 可接受链接;
     * channel 可接收数据; 如果读到数据长度为 0, 则是读通道关闭事件
     */
    virtual void handle_read_ready() = 0;

    /**
     * channel 可发送数据
     */
    virtual void handle_write_ready() = 0;

private:
    ReactHandler(const ReactHandler&) = delete;
    ReactHandler& operator=(const ReactHandler&) = delete;

private:
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    // 用于记录注册状态，参见 Reactor 的实现
    int _registered_events = 0;
#endif
};

}

#endif
