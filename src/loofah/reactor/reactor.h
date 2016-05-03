

#ifndef ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_
#define ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_

#include <nut/platform/platform.h>

#include "sync_event_handler.h"

namespace loofah
{

class Reactor
{
#if NUT_PLATFORM_OS_WINDOWS
    // TODO
#elif NUT_PLATFORM_OS_LINUX
	int _epoll_fd = -1;
#elif NUT_PLATFORM_OS_MAC
    int _kq = -1;
#endif

public:
    Reactor();

    void register_handler(SyncEventHandler *handler, int mask);
    void unregister_handler(SyncEventHandler *handler);

	void enable_handler(SyncEventHandler *handler, int mask);
	void disable_handler(SyncEventHandler *handler, int mask);

    void handle_events(int timeout_ms=1000);
};

}

#endif
