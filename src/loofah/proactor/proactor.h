
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include "proact_handler.h"

namespace loofah
{

class LOOFAH_API Proactor
{
#if NUT_PLATFORM_OS_WINDOWS
    HANDLE _iocp = INVALID_HANDLE_VALUE;
#elif NUT_PLATFORM_OS_MAC
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    int _epoll_fd = -1;
#endif

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    void enable_handler(ProactHandler *handler, int mask);
    void disable_handler(ProactHandler *handler, int mask);
#endif

public:
    Proactor();
    ~Proactor();

    void register_handler(ProactHandler *handler);

    void launch_accept(ProactHandler *handler);
    void launch_read(ProactHandler *handler, void *buf, int buf_len);
    void launch_write(ProactHandler *handler, void *buf, int buf_len);

    /**
     * @param timeout_ms 超时毫秒数，在 Windows 下可传入 INFINITE 表示无穷等待
     */
    void handle_events(int timeout_ms = 1000);
};

}

#endif
