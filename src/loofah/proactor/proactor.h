
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include <nut/platform/platform.h>

#include "async_event_handler.h"

namespace loofah
{

class Proactor
{
#if defined(NUT_PLATFORM_OS_MAC)
    int _kq = 0;
#endif

public:
    Proactor();

    void register_handler(AsyncEventHandler *handler, int mask);
    void unregister_handler(AsyncEventHandler *handler, int mask);

    void handle_events(int timeout_ms=1000);
};

}

#endif
