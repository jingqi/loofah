
#ifndef ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_
#define ___HEADFILE_29BEE4A7_4FDA_4FAB_ABC2_3FC8E8A22FD4_

#include "../loofah_config.h"

#include <unordered_map>
#include <atomic>

#include <nut/platform/platform.h>

#include "../inet_base/poller_base.h"
#include "react_handler.h"


namespace loofah
{

class LOOFAH_API Reactor : public PollerBase
{
public:
    Reactor() noexcept;
    virtual ~Reactor() noexcept override;

    void register_handler(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;
    void register_handler_later(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;

    void unregister_handler(ReactHandler *handler) noexcept;
    void unregister_handler_later(ReactHandler *handler) noexcept;

    void enable_handler(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;
    void enable_handler_later(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;

    void disable_handler(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;
    void disable_handler_later(ReactHandler *handler, ReactHandler::mask_type mask) noexcept;

    /**
     * 关闭 reactor
     */
    void shutdown_later() noexcept;

    /**
     * @param timeout_ms <0 无限等待; >=0 等待超时的毫秒数
     * @return 0 表示正常; <0 表示出错
     */
    int poll(int timeout_ms = 1000) noexcept;

    virtual void wakeup_poll_wait() noexcept final override;

protected:
#if NUT_PLATFORM_OS_WINDOWS && WINVER >= _WIN32_WINNT_WINBLUE
    void ensure_capacity(size_t new_size) noexcept;
    ssize_t binary_search(ReactHandler *handler) noexcept;
#endif

    void shutdown() noexcept;

private:
#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    // Windows 8.1 之前，使用 ::select() 实现
    // NOTE 默认情况下受限于 FD_SETSIZE = 64 大小限制, 但是可以在包含 winsock2.h 之前
    //      define 其为更大的值, 参见 https://msdn.microsoft.com/zh-cn/library/windows/desktop/ms740141(v=vs.85).aspx
    fd_set _read_set, _write_set, _except_set;
    std::unordered_map<socket_t, ReactHandler*> _socket_to_handler;
#elif NUT_PLATFORM_OS_WINDOWS
    // Windows 8.1 及其之后，使用 ::WSAPoll() 实现
    WSAPOLLFD *_pollfds = nullptr;
    ReactHandler **_handlers = nullptr;
    size_t _capacity = 16;
    size_t _size = 0;
#elif NUT_PLATFORM_OS_MACOS
    // 使用 ::kqueue() 实现
    int _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    // 使用 ::epoll() 实现
    int _epoll_fd = -1;
    bool _edge_triggered = false; // level-triggered or edge-triggered
    int _event_fd = -1;
#endif

    std::atomic<bool> _closing_or_closed = ATOMIC_VAR_INIT(false);
};

}

#endif
