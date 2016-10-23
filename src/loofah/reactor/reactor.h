

#ifndef ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_
#define ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_

#include "../loofah.h"

#include <map>

#include <nut/platform/platform.h>
#include <nut/threading/sync/mutex.h>

#include "react_handler.h"


namespace loofah
{

class LOOFAH_API Reactor
{
#if NUT_PLATFORM_OS_WINDOWS
    nut::Mutex _mutex;
    FD_SET _read_set, _write_set, _except_set;
    std::map<socket_t, ReactHandler*> _socket_to_handler;
#elif NUT_PLATFORM_OS_MAC
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    int _epoll_fd = -1;
    bool _edge_triggered = false; // level-triggered or edge-triggered
#endif

    bool _closed = false;

public:
    Reactor();
    ~Reactor();

    void register_handler(ReactHandler *handler, int mask);
    void unregister_handler(ReactHandler *handler);

    void enable_handler(ReactHandler *handler, int mask);
    void disable_handler(ReactHandler *handler, int mask);

    /**
     * @return <0 出错
     */
    int handle_events(int timeout_ms = 1000);
    void shutdown();
};

}

#endif
