

#ifndef ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_
#define ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_

#include <nut/platform/platform.h>

#include "sync_event_handler.h"

namespace loofah
{

class Reactor
{
#if defined(NUT_PLATFORM_OS_WINDOWS)
    // TODO
#elif defined(NUT_PLATFORM_OS_MAC)
    int _kq = 0;
#else
    // TODO
#endif

public:
    Reactor();

    void register_handler(SyncEventHandler *handler, int mask);
    void unregister_handler(SyncEventHandler *handler, int mask);

	void enable_handler(SyncEventHandler *handler, int mask);
	void disable_handler(SyncEventHandler *handler, int mask);

    void handle_events(int timeout_ms=1000);
};

}

#endif
