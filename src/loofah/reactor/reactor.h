

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
#if NUT_PLATFORM_OS_WINDOWS
    FD_SET _read_set, _write_set, _except_set;
    std::map<socket_t, ReactHandler*> _socket_to_handler;
#elif NUT_PLATFORM_OS_MAC
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    int _epoll_fd = -1;
    bool _edge_triggered = false; // level-triggered or edge-triggered
#endif

    bool _closing_or_closed = false;

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

protected:
    void register_handler(ReactHandler *handler, int mask);
    void unregister_handler(ReactHandler *handler);

    void enable_handler(ReactHandler *handler, int mask);
    void disable_handler(ReactHandler *handler, int mask);

    void shutdown();

public:
    Reactor();
    ~Reactor();

    void async_register_handler(ReactHandler *handler, int mask);
    void async_unregister_handler(ReactHandler *handler);

    void async_enable_handler(ReactHandler *handler, int mask);
    void async_disable_handler(ReactHandler *handler, int mask);

    /**
     * 关闭 reactor
     */
    void async_shutdown();

    /**
     * @return <0 出错
     */
    int handle_events(int timeout_ms = 1000);
};

}

#endif
