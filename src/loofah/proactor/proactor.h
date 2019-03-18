
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include "../loofah_config.h"

#include <unordered_set>
#include <atomic>

#include <nut/platform/platform.h>

#include "../inet_base/event_loop_base.h"
#include "../inet_base/inet_addr.h"
#include "proact_handler.h"


namespace loofah
{

class LOOFAH_API Proactor : public EventLoopBase
{
public:
    Proactor();
    ~Proactor();

    void register_handler(ProactHandler *handler);
    void register_handler_later(ProactHandler *handler);

    void unregister_handler(ProactHandler *handler);
    void unregister_handler_later(ProactHandler *handler);

    void launch_accept(ProactHandler *handler);
    void launch_accept_later(ProactHandler *handler);

#if NUT_PLATFORM_OS_WINDOWS
    void launch_connect(ProactHandler *handler, const InetAddr& address);
    void launch_connect_later(ProactHandler *handler, const InetAddr& address);
#else
    void launch_connect(ProactHandler *handler);
    void launch_connect_later(ProactHandler *handler);
#endif

    void launch_read(ProactHandler *handler, void* const *buf_ptrs,
                     const size_t *len_ptrs, size_t buf_count);
    void launch_read_later(ProactHandler *handler, void* const *buf_ptrs,
                           const size_t *len_ptrs, size_t buf_count);

    void launch_write(ProactHandler *handler, void* const *buf_ptrs,
                      const size_t *len_ptrs, size_t buf_count);
    void launch_write_later(ProactHandler *handler, void* const *buf_ptrs,
                            const size_t *len_ptrs, size_t buf_count);

    /**
     * 关闭 proactor
     */
    void shutdown_later();

    /**
     * @param timeout_ms 超时毫秒数，在 Windows 下可传入 INFINITE 表示无穷等待
     * @return <0 出错
     */
    int handle_events(int timeout_ms = 1000);

protected:
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    void enable_handler(ProactHandler *handler, ProactHandler::mask_type mask);
    void disable_handler(ProactHandler *handler, ProactHandler::mask_type mask);
#endif

    void shutdown();

private:
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 函数 ::CreateIoCompletionPort() 将 nullptr 作为失败返回值, 而不是
    //      INVALID_HANDLE_VALUE, 所以需要将 nullptr 作为无效值
    HANDLE _iocp = nullptr;
    std::unordered_set<socket_t> _associated_sockets;
#elif NUT_PLATFORM_OS_MAC
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    int _epoll_fd = -1;
#endif

    std::atomic<bool> _closing_or_closed = ATOMIC_VAR_INIT(false);
};

}

#endif
