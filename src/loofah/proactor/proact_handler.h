﻿
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

class LOOFAH_API ProactHandler
{
    NUT_REF_COUNTABLE

    friend class Proactor;

public:
    enum EventType
    {
        ACCEPT_MASK = 0x01,
        READ_MASK = 0x02,
        WRITE_MASK = 0x04,
        EXCEPT_MASK = 0x08,
    };

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
     * channel 收到数据
     */
    virtual void handle_read_completed(ssize_t cb) = 0;

    /**
     * channel 发送数据
     */
    virtual void handle_write_completed(ssize_t cb) = 0;

private:
    ProactHandler(const ProactHandler&) = delete;
    ProactHandler& operator=(const ProactHandler&) = delete;

private:
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    int _registered_events = 0; // 用于记录注册状态，参见 Proactor 的实现
    int _request_accept = 0;
    std::queue<IORequest*> _read_queue, _write_queue;
#endif
};

}

#endif
