
#ifndef ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_
#define ___HEADFILE_54F6D416_3E0D_4941_AF22_B260CD34E51B_

#include "../loofah_config.h"

#include <unordered_map>
#include <atomic>

#include <nut/platform/platform.h>

#include "proact_handler.h"
#include "../inet_base/poller_base.h"
#include "../inet_base/inet_addr.h"


namespace loofah
{

class LOOFAH_API Proactor : public PollerBase
{
public:
    Proactor() = default;
    virtual ~Proactor() noexcept override;

    /**
     * 初始化
     */
    bool initialize() noexcept;

    /**
     * 关闭 proactor
     */
    void shutdown() noexcept;

    void register_handler(ProactHandler *handler) noexcept;
    void unregister_handler(ProactHandler *handler) noexcept;

    void launch_accept(ProactHandler *handler) noexcept;

#if NUT_PLATFORM_OS_WINDOWS
    void launch_connect(ProactHandler *handler, const InetAddr& address) noexcept;
#else
    void launch_connect(ProactHandler *handler) noexcept;
#endif

    void launch_read(ProactHandler *handler, void* const *buf_ptrs,
                     const size_t *len_ptrs, size_t buf_count) noexcept;

    void launch_write(ProactHandler *handler, void* const *buf_ptrs,
                      const size_t *len_ptrs, size_t buf_count) noexcept;

    /**
     * @param timeout_ms <0 表示无限等待; >=0 等待超时的毫秒数
     * @return 0 表示正常; <0 表示出错
     */
    int poll(int timeout_ms = 1000) noexcept;

    virtual void wakeup_poll_wait() noexcept final override;

protected:
#if NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
    void enable_handler(ProactHandler *handler, ProactHandler::mask_type mask) noexcept;
    void disable_handler(ProactHandler *handler, ProactHandler::mask_type mask) noexcept;
#endif

private:
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 函数 ::CreateIoCompletionPort() 将 nullptr 作为失败返回值, 而不是
    //      INVALID_HANDLE_VALUE, 所以需要将 nullptr 作为无效值
    HANDLE _iocp = nullptr;
#elif NUT_PLATFORM_OS_MACOS
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    int _epoll_fd = -1;
#endif

#if NUT_PLATFORM_OS_LINUX
    // eventfd
    int _event_fd = -1;
#endif

    std::unordered_map<socket_t, nut::rc_ptr<ProactHandler>> _handlers;
};

}

#endif
