
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include <vector>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include <nut/threading/runnable.h>
#include <nut/threading/thread.h>
#include <nut/threading/sync/mutex.h>

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

    nut::Thread::tid_type _loop_tid;
    std::vector<nut::rc_ptr<nut::Runnable> > _async_tasks;
    nut::Mutex _mutex;

    bool _closing_or_closed = false;

protected:
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    void enable_handler(ProactHandler *handler, int mask);
    void disable_handler(ProactHandler *handler, int mask);
#endif

    void register_handler(ProactHandler *handler);

    void launch_accept(ProactHandler *handler);
    void launch_read(ProactHandler *handler, void* const *buf_ptrs,
                     const size_t *len_ptrs, size_t buf_count);
    void launch_write(ProactHandler *handler, void* const *buf_ptrs,
                      const size_t *len_ptrs, size_t buf_count);

    void shutdown();

public:
    Proactor();
    ~Proactor();

    void async_register_handler(ProactHandler *handler);

    void async_launch_accept(ProactHandler *handler);
    void async_launch_read(ProactHandler *handler, void* const *buf_ptrs,
                           const size_t *len_ptrs, size_t buf_count);
    void async_launch_write(ProactHandler *handler, void* const *buf_ptrs,
                            const size_t *len_ptrs, size_t buf_count);

    /**
     * 关闭 proactor
     */
    void async_shutdown();

    /**
     * 在事件循环线程中运行
     */
    void run_in_loop_thread(nut::Runnable *runnable);

    /**
     * @param timeout_ms 超时毫秒数，在 Windows 下可传入 INFINITE 表示无穷等待
     * @return <0 出错
     */
    int handle_events(int timeout_ms = 1000);
};

}

#endif
