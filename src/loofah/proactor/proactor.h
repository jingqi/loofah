
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#    include <winsock2.h>
#    include <windows.h>
#endif

#include "async_event_handler.h"

namespace loofah
{

class Proactor
{
#if NUT_PLATFORM_OS_WINDOWS
    HANDLE _iocp = INVALID_HANDLE_VALUE;
#elif NUT_PLATFORM_OS_MAC
    int _kq = 0;
#endif

public:
    Proactor();
    ~Proactor();

#if NUT_PLATFORM_OS_WINDOWS
    HANDLE get_iocp()
    {
        return _iocp;
    }
#endif

    void register_handler(AsyncEventHandler *handler);

    void launch_accept(AsyncEventHandler *handler);
    void launch_read(AsyncEventHandler *handler, int buf_len);
    void launch_write(AsyncEventHandler *handler, const void *buf, int buf_len);

    void handle_events(int timeout_ms=1000);
};

}

#endif
