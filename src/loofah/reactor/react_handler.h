
#ifndef ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_
#define ___HEADFILE_51392254_9561_4A97_A283_F00E6C58F874_

#include "../loofah_config.h"

#include <stdint.h>

#include <nut/rc/rc_ptr.h>


namespace loofah
{

class Reactor;

class ReactHandler
{
    NUT_REF_COUNTABLE

    friend class Reactor;

public:
    enum EventType
    {
        ACCEPT_MASK = 0x01,
        CONNECT_MASK = 0x02,
        READ_MASK = 0x04,
        WRITE_MASK = 0x08,

        ACCEPT_READ_MASK = ACCEPT_MASK | READ_MASK,
        CONNECT_WRITE_MASK = CONNECT_MASK | WRITE_MASK,
        READ_WRITE_MASK = READ_MASK | WRITE_MASK,
        ALL_MASK = ACCEPT_MASK | CONNECT_MASK | READ_MASK | WRITE_MASK,
    };

    typedef uint8_t mask_type;

public:
    ReactHandler() = default;

    virtual ~ReactHandler() = default;

    virtual socket_t get_socket() const noexcept = 0;

    /**
     * acceptor 有新链接
     */
    virtual void handle_accept_ready() noexcept = 0;

    /**
     * connector 即将完成
     */
    virtual void handle_connect_ready() noexcept = 0;

    /**
     * channel 可接收数据; 如果读到数据长度为 0, 则是读通道关闭事件
     */
    virtual void handle_read_ready() noexcept = 0;

    /**
     * channel 可发送数据
     */
    virtual void handle_write_ready() noexcept = 0;

    /**
     * 出错
     *
     * @param err 错误码，如 LOOFAH_ERR_PKG_OVERSIZE 等
     */
    virtual void handle_io_error(int err) noexcept = 0;

private:
    ReactHandler(const ReactHandler&) = delete;
    ReactHandler& operator=(const ReactHandler&) = delete;

protected:
    Reactor *_registered_reactor = nullptr;

private:
    // ACCEPT_MASK, CONNECT_MASK, READ_MASK, WRITE_MASK
    mask_type _enabled_events = 0;

    // 用于记录注册状态，参见 Reactor 的实现
#if NUT_PLATFORM_OS_WINDOWS && WINVER >= _WIN32_WINNT_WINBLUE
    bool _registered = false;
#elif NUT_PLATFORM_OS_MACOS
    // READ_MASK, WRITE_MASK
    mask_type _registered_events = 0;
#elif NUT_PLATFORM_OS_LINUX
    bool _registered = false;
#endif
};

}

#endif
