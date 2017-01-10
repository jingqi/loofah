
#ifndef ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_
#define ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_

#include "../loofah_config.h"

#include <queue>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif

#include <nut/threading/sync/mutex.h>
#include <nut/rc/rc_ptr.h>


namespace loofah
{

class IORequest;

class LOOFAH_API ProactHandler
{
    NUT_REF_COUNTABLE

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    int _registered_events = 0; // 用于记录注册状态，参见 Proactor 的实现
    int _request_accept = 0;
    std::queue<IORequest*> _read_queue, _write_queue;
    friend class Proactor;
#endif

public:
    enum EventType
    {
        ACCEPT_MASK = 1,
        READ_MASK = 1 << 1,
        WRITE_MASK = 1 << 2,
        EXCEPT_MASK = 1 << 3,
    };

    virtual ~ProactHandler();

    virtual socket_t get_socket() const = 0;

    /**
     * acceptor 收到链接
     */
    virtual void handle_accept_completed(socket_t fd) = 0;

    /**
     * channel 收到数据
     */
    virtual void handle_read_completed(int cb) = 0;

    /**
     * channel 发送数据
     */
    virtual void handle_write_completed(int cb) = 0;
};

}

#endif
