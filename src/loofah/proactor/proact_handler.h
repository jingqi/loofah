
#ifndef ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_
#define ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_

#include "../loofah_config.h"

#include <queue>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif

#include <nut/platform/int_type.h> /* for ssize_t */
#include <nut/rc/rc_ptr.h>


namespace loofah
{

class IORequest;
class Proactor;

class LOOFAH_API ProactHandler
{
    NUT_REF_COUNTABLE

    friend class Proactor;

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
    ProactHandler() = default;

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    virtual ~ProactHandler();
#endif

    virtual socket_t get_socket() const = 0;

    /**
     * acceptor 收到链接
     */
    virtual void handle_accept_completed(socket_t fd) = 0;

    /**
     * connector 连接完成
     */
    virtual void handle_connect_completed() = 0;

    /**
     * channel 收到数据; 如果 cb==0, 则是读通道关闭事件
     *
     * @param cb >0 收到的字节数
     *            0 读通道关闭
     */
    virtual void handle_read_completed(size_t cb) = 0;

    /**
     * channel 发送数据
     *
     * @param cb 写入的字节数
     */
    virtual void handle_write_completed(size_t cb) = 0;

    /**
     * channel 出错
     *
     * @param err 错误码, 如 LOOFAH_ERR_PKG_OVERSIZE 等
     */
    virtual void handle_io_exception(int err) = 0;

private:
    ProactHandler(const ProactHandler&) = delete;
    ProactHandler& operator=(const ProactHandler&) = delete;

protected:
    Proactor *_proactor = nullptr;

private:
#if NUT_PLATFORM_OS_MAC | NUT_PLATFORM_OS_LINUX
    // ACCEPT_MASK, CONNECT_MASK, READ_MASK, WRITE_MASK
    mask_type _enabled_events = 0;

    // accept 请求数
    size_t _request_accept = 0;

    // 读写请求队列
    std::queue<IORequest*> _read_queue, _write_queue;
#endif

    // 用于记录注册状态，参见 Proactor 的实现
#if NUT_PLATFORM_OS_MAC
    // READ_MASK, WRITE_MASK
    mask_type _registered_events = 0;
#elif NUT_PLATFORM_OS_LINUX
    bool _registered = false;
#endif
};

}

#endif
