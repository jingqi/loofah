﻿

#ifndef ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_
#define ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_

#include "../loofah_config.h"

#include <map>

#include <nut/platform/platform.h>

#include "../inet_base/event_loop_base.h"
#include "react_handler.h"


namespace loofah
{

class LOOFAH_API Reactor : public EventLoopBase
{
public:
    enum
    {
        // 关闭 socket 前是否需要 unregister_handler()
#if NUT_PLATFORM_OS_WINDOWS
        NEED_UNREGISTER_BEFORE_CLOSE = true,
#else
        NEED_UNREGISTER_BEFORE_CLOSE = false,
#endif
    };

public:
    Reactor();
    ~Reactor();

    void register_handler_later(ReactHandler *handler, int mask);
    void unregister_handler_later(ReactHandler *handler);

    void enable_handler_later(ReactHandler *handler, int mask);
    void disable_handler_later(ReactHandler *handler, int mask);

    /**
     * 关闭 reactor
     */
    void shutdown_later();

    /**
     * @return <0 出错
     */
    int handle_events(int timeout_ms = 1000);

protected:
#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER >= _WIN32_WINNT_WINBLUE
    void ensure_capacity(size_t new_size);
    size_t index_of(ReactHandler *handler);
#   endif
#endif

    void register_handler(ReactHandler *handler, int mask);
    void unregister_handler(ReactHandler *handler);

    void enable_handler(ReactHandler *handler, int mask);
    void disable_handler(ReactHandler *handler, int mask);

    void shutdown();

private:
#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE // Windows8.1, 0x0603
    // Windows 8.1 之前，使用 ::select() 实现
    // NOTE 默认情况下受限于 FD_SETSIZE = 64 大小限制, 但是可以在包含 winsock2.h 之前
    //      define 其为更大的值, 参见 https://msdn.microsoft.com/zh-cn/library/windows/desktop/ms740141(v=vs.85).aspx
    FD_SET _read_set, _write_set, _except_set;
    std::map<socket_t, ReactHandler*> _socket_to_handler;
#   else
    // Windows 8.1 之后，使用 ::WSAPoll() 实现
    WSAPOLLFD *_pollfds = nullptr;
    ReactHandler **_handlers = nullptr;
    size_t _capacity = 16;
    size_t _size = 0;
#   endif
#elif NUT_PLATFORM_OS_MAC
    // 使用 ::kqueue() 实现
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    // 使用 ::epoll() 实现
    int _epoll_fd = -1;
    bool _edge_triggered = false; // level-triggered or edge-triggered
#endif

    bool volatile _closing_or_closed = false;
};

}

#endif
