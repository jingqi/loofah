
#include "../loofah_config.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#elif NUT_PLATFORM_OS_MACOS
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h>
#   include <sys/eventfd.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#endif

#include <assert.h>
#include <string.h>

#include <nut/logging/logger.h>

#include "../inet_base/error.h"
#include "../inet_base/sock_operation.h"
#include "reactor.h"


// Magic number
#define KQUEUE_WAKEUP_IDENT 0

#define TAG "loofah.reactor"

namespace loofah
{

namespace
{

#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE && !defined(FD_COPY)
void FD_COPY(fd_set *dst, const fd_set *src)
{
    assert(nullptr != dst && nullptr != src);
    const unsigned count = src->fd_count;
    ::memcpy(dst->fd_array, src->fd_array, sizeof(src->fd_array[0]) * count);
    dst->fd_count = count;
}
#endif

ReactHandler::mask_type real_mask(ReactHandler::mask_type mask)
{
    ReactHandler::mask_type ret = 0;
    if (0 != (mask & ReactHandler::ACCEPT_READ_MASK))
        ret |= ReactHandler::READ_MASK;
    if (0 != (mask & ReactHandler::CONNECT_WRITE_MASK))
        ret |= ReactHandler::WRITE_MASK;
    return ret;
}

}

Reactor::Reactor()
{
#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
#elif NUT_PLATFORM_OS_WINDOWS
    _pollfds = (WSAPOLLFD*) ::malloc(sizeof(WSAPOLLFD) * _capacity);
    _handlers = (ReactHandler**) ::malloc(sizeof(ReactHandler*) * _capacity);
#elif NUT_PLATFORM_OS_MACOS
    // Create kqueue
    _kq = ::kqueue();
    if (_kq < 0)
    {
        LOOFAH_LOG_ERRNO(kqueue);
        return;
    }

    // Register user event filter
    struct kevent ev;
    EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, NOTE_FFNOP, 0, nullptr);
    if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
        LOOFAH_LOG_FD_ERRNO(kevent, _kq);
#elif NUT_PLATFORM_OS_LINUX
    // Create epoll fd
    // NOTE 自从 Linux2.6.8 版本以后，epoll_create() 参数值其实是没什么用的, 只
    //      需要大于 0
    _epoll_fd = ::epoll_create(2048);
    if (_epoll_fd < 0)
    {
        LOOFAH_LOG_ERRNO(epoll_create);
        return;
    }

    // Create eventfd
    _event_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (_event_fd < 0)
    {
        LOOFAH_LOG_ERRNO(eventfd);
        return;
    }

    // Register eventfd to epoll fd
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.fd = _event_fd;
    epv.events = EPOLLIN | EPOLLERR;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _event_fd, &epv))
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, _event_fd);
#endif
}

Reactor::~Reactor()
{
    shutdown();

#if NUT_PLATFORM_OS_WINDOWS && WINVER >= _WIN32_WINNT_WINBLUE
    if (nullptr != _pollfds)
        ::free(_pollfds);
    _pollfds = nullptr;
    if (nullptr != _handlers)
        ::free(_handlers);
    _handlers = nullptr;
#endif
}

void Reactor::shutdown_later()
{
    _closing_or_closed.store(true, std::memory_order_relaxed);

    if (!is_in_io_thread())
        wakeup_poll_wait();
}

void Reactor::shutdown()
{
    assert(is_in_io_thread());

    _closing_or_closed.store(true, std::memory_order_relaxed);

#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
    _socket_to_handler.clear();
#elif NUT_PLATFORM_OS_WINDOWS
    _size = 0;
#elif NUT_PLATFORM_OS_MACOS
    if (_kq >= 0)
    {
        // Unregister user event filter
        struct kevent ev;
        EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, EV_DELETE, 0, 0, nullptr);
        if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
            LOOFAH_LOG_FD_ERRNO(kevent, _kq);

        // Close kqueue
        if (0 != ::close(_kq))
            LOOFAH_LOG_ERRNO(close);
    }
    _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    // Unregister eventfd from epoll fd
    if (_event_fd >= 0 && _epoll_fd >= 0)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.fd = _event_fd;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _event_fd, &epv))
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, _event_fd);
    }

    // Close eventfd
    if (_event_fd >= 0)
    {
        if (0 != ::close(_event_fd))
            LOOFAH_LOG_ERRNO(close);
    }
    _event_fd = -1;

    // Close epoll fd
    if (_epoll_fd >= 0)
    {
        if (0 != ::close(_epoll_fd))
            LOOFAH_LOG_ERRNO(close);
    }
    _epoll_fd = -1;
#endif
}

#if NUT_PLATFORM_OS_WINDOWS && WINVER >= _WIN32_WINNT_WINBLUE
void Reactor::ensure_capacity(size_t new_size)
{
    if (new_size <= _capacity)
        return;

    size_t new_cap = _size * 3 / 2;
    if (new_cap < new_size)
        new_cap = new_size;

    WSAPOLLFD *new_pollfds = (WSAPOLLFD*) ::realloc(_pollfds, sizeof(WSAPOLLFD) * new_cap);
    ReactHandler ** new_handlers = (ReactHandler**) ::realloc(_handlers, sizeof(ReactHandler*) * new_cap);
    assert(nullptr != new_pollfds && nullptr != new_handlers);
    _pollfds = new_pollfds;
    _handlers = new_handlers;
    _capacity = new_cap;
}

ssize_t Reactor::binary_search(ReactHandler *handler)
{
    ssize_t left = -1, right = _size;
    while (left + 1 < right)
    {
        size_t middle = (left + right) >> 1;
        if (_handlers[middle] == handler)
            return middle;
        else if (_handlers[middle] < handler)
            left = middle;
        else
            right = middle;
    }
    return -right - 1;
}
#endif

void Reactor::register_handler_later(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        register_handler(handler, mask);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactHandler> ref_handler(handler);
        add_later_task([=] { register_handler(ref_handler, mask); });
    }
}

void Reactor::register_handler(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler && 0 == (mask & ~ReactHandler::ALL_MASK));
    assert(nullptr == handler->_registered_reactor && 0 == handler->_enabled_events);
    assert(is_in_io_thread());

#if NUT_PLATFORM_OS_WINDOWS && WINVER >= _WIN32_WINNT_WINBLUE
    assert(!handler->_registered);
#elif NUT_PLATFORM_OS_MACOS
    assert(0 == handler->_registered_events);
#elif NUT_PLATFORM_OS_LINUX
    assert(!handler->_registered);
#endif

    handler->_registered_reactor = this;

    enable_handler(handler, mask);
}

void Reactor::unregister_handler_later(ReactHandler *handler)
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        unregister_handler(handler);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactHandler> ref_handler(handler);
        add_later_task([=] { unregister_handler(ref_handler); });
    }
}

void Reactor::unregister_handler(ReactHandler *handler)
{
    assert(nullptr != handler && handler->_registered_reactor == this);
    assert(is_in_io_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    if (0 != (handler->_enabled_events & ReactHandler::ACCEPT_READ_MASK))
        FD_CLR(fd, &_read_set);
    if (0 != (handler->_enabled_events & ReactHandler::CONNECT_WRITE_MASK))
        FD_CLR(fd, &_write_set);
    if (0 != handler->_enabled_events)
        FD_CLR(fd, &_except_set);
    _socket_to_handler.erase(fd);
    handler->_enabled_events = 0;
    handler->_registered_reactor = nullptr;
#elif NUT_PLATFORM_OS_WINDOWS
    if (handler->_registered)
    {
        const ssize_t index = binary_search(handler);
        assert(index >= 0);
        if (index + 1 < _size)
        {
            ::memmove(_pollfds + index, _pollfds + index + 1, sizeof(WSAPOLLFD) * (_size - index - 1));
            ::memmove(_handlers + index, _handlers + index + 1, sizeof(ReactHandler*) * (_size - index - 1));
        }
        --_size;
    }
    handler->_registered = false;
    handler->_enabled_events = 0;
    handler->_registered_reactor = nullptr;
#elif NUT_PLATFORM_OS_MACOS
    struct kevent ev[2];
    int n = 0;
    if (0 != (handler->_registered_events & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    if (0 != (handler->_registered_events & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_registered_events = 0;
    handler->_enabled_events = 0;
    handler->_registered_reactor = nullptr;
#elif NUT_PLATFORM_OS_LINUX
    if (handler->_registered)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_registered = false;
    handler->_enabled_events = 0;
    handler->_registered_reactor = nullptr;
#endif
}

void Reactor::enable_handler_later(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        enable_handler(handler, mask);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactHandler> ref_handler(handler);
        add_later_task([=] { enable_handler(ref_handler, mask); });
    }
}

void Reactor::enable_handler(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler && 0 == (mask & ~ReactHandler::ALL_MASK));
    assert(handler->_registered_reactor == this);
    assert(is_in_io_thread());

    if (0 == mask)
        return;
    const socket_t fd = handler->get_socket();

    // NOTE
    // - 异步 accept
    //    - 有可读事件
    // - 异步 connect
    //   - 成功，有可写事件
    //   - 失败，有可读、可写事件
#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    const ReactHandler::mask_type need_enable = ~real_mask(handler->_enabled_events) & real_mask(mask);
    if (0 != (need_enable & ReactHandler::READ_MASK))
        FD_SET(fd, &_read_set);
    if (0 != (need_enable & ReactHandler::WRITE_MASK))
        FD_SET(fd, &_write_set);
    if (0 == handler->_enabled_events)
        FD_SET(fd, &_except_set);
    _socket_to_handler.emplace(fd, handler);
    handler->_enabled_events |= mask;
#elif NUT_PLATFORM_OS_WINDOWS
    ssize_t index = binary_search(handler);
    if (!handler->_registered)
    {
        assert(index < 0);
        index = -index - 1;
        ensure_capacity(_size + 1);
        if (index < _size)
        {
            ::memmove(_pollfds + index + 1, _pollfds + index, sizeof(WSAPOLLFD) * (_size - index));
            ::memmove(_handlers + index + 1, _handlers + index, sizeof(ReactHandler*) * (_size - index));
        }
        _handlers[index] = handler;
        _pollfds[index].fd = fd;
        _pollfds[index].events = 0;
        _pollfds[index].revents = 0;
        ++_size;
    }
    assert(index >= 0);

    if (0 != (mask & ReactHandler::ACCEPT_READ_MASK))
        _pollfds[index].events |= POLLIN;
    if (0 != (mask & ReactHandler::CONNECT_WRITE_MASK))
        _pollfds[index].events |= POLLOUT;
    handler->_registered = true;
    handler->_enabled_events |= mask;
#elif NUT_PLATFORM_OS_MACOS
    struct kevent ev[2];
    int n = 0;
    const ReactHandler::mask_type need_enable = ~real_mask(handler->_enabled_events) & real_mask(mask);
    if (0 != (need_enable & ReactHandler::READ_MASK))
    {
        if (0 != (handler->_registered_events & ReactHandler::READ_MASK))
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (0 != (need_enable & ReactHandler::WRITE_MASK))
    {
        if (0 != (handler->_registered_events & ReactHandler::WRITE_MASK))
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_registered_events |= need_enable;
    handler->_enabled_events |= mask;
#elif NUT_PLATFORM_OS_LINUX
    const ReactHandler::mask_type need_enable = ~real_mask(handler->_enabled_events) & real_mask(mask);
    if (0 != need_enable)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        const ReactHandler::mask_type final_enabled = handler->_enabled_events | mask;
        if (0 != (final_enabled & ReactHandler::ACCEPT_READ_MASK))
            epv.events |= EPOLLIN | EPOLLERR;
        if (0 != (final_enabled & ReactHandler::CONNECT_WRITE_MASK))
            epv.events |= EPOLLOUT | EPOLLERR;
        if (_edge_triggered)
            epv.events |= EPOLLET;
        if (0 != ::epoll_ctl(_epoll_fd, (handler->_registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD), fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_registered = true;
    handler->_enabled_events |= mask;
#endif
}

void Reactor::disable_handler_later(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        disable_handler(handler, mask);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactHandler> ref_handler(handler);
        add_later_task([=] { disable_handler(ref_handler, mask); });
    }
}

void Reactor::disable_handler(ReactHandler *handler, ReactHandler::mask_type mask)
{
    assert(nullptr != handler && 0 == (mask & ~ReactHandler::ALL_MASK));
    assert(handler->_registered_reactor == this);
    assert(is_in_io_thread());

    if (0 == mask)
        return;
    const socket_t fd = handler->get_socket();
    const ReactHandler::mask_type final_enabled = real_mask(handler->_enabled_events & ~mask),
        need_disable = real_mask(handler->_enabled_events) & ~final_enabled;

#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    if (0 != (need_disable & ReactHandler::READ_MASK))
        FD_CLR(fd, &_read_set);
    if (0 != (need_disable & ReactHandler::WRITE_MASK))
        FD_CLR(fd, &_write_set);
    if (0 != handler->_enabled_events && 0 == final_enabled)
        FD_CLR(fd, &_except_set);
    handler->_enabled_events &= ~mask;
#elif NUT_PLATFORM_OS_WINDOWS
    if (0 != need_disable)
    {
        const ssize_t index = binary_search(handler);
        assert(index >= 0);
        if (0 != (need_disable & ReactHandler::READ_MASK))
            _pollfds[index].events &= ~POLLIN;
        if (0 != (need_disable & ReactHandler::WRITE_MASK))
            _pollfds[index].events &= ~POLLOUT;
    }
    handler->_enabled_events &= ~mask;
#elif NUT_PLATFORM_OS_MACOS
    struct kevent ev[2];
    int n = 0;
    if (0 != (need_disable & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (need_disable & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_enabled_events &= ~mask;
#elif NUT_PLATFORM_OS_LINUX
    if (0 != need_disable)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        if (0 != (final_enabled & ReactHandler::READ_MASK))
            epv.events |= EPOLLIN | EPOLLERR;
        if (0 != (final_enabled & ReactHandler::WRITE_MASK))
            epv.events |= EPOLLOUT | EPOLLERR;
        if (_edge_triggered)
            epv.events |= EPOLLET;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_enabled_events &= ~mask;
#endif
}

int Reactor::poll(int timeout_ms)
{
    if (_closing_or_closed.load(std::memory_order_relaxed))
        return -1;

    if (!is_in_io_thread())
    {
        NUT_LOG_F(TAG, "poll() can only be called from inside IO thread");
        return -1;
    }

#if NUT_PLATFORM_OS_WINDOWS && WINVER < _WIN32_WINNT_WINBLUE
    struct timeval timeout;
    if (timeout_ms < 0)
    {
        timeout.tv_sec = 31536000;
        timeout.tv_usec = 999999999;
    }
    else
    {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
    }

    fd_set read_set, write_set, except_set;
    FD_COPY(&read_set, &_read_set);
    FD_COPY(&write_set, &_write_set);
    FD_COPY(&except_set, &_except_set);

    _poll_stage = PollStage::PollingWait;
    const int rs = ::select(0, &read_set, &write_set, &except_set, &timeout);
    _poll_stage = PollStage::HandlingEvents;
    if (SOCKET_ERROR == rs)
    {
        LOOFAH_LOG_ERRNO(select);
        return -1;
    }
    for (unsigned i = 0; i < read_set.fd_count; ++i)
    {
        const socket_t fd = read_set.fd_array[i];
        std::unordered_map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
        if (iter == _socket_to_handler.end())
            continue;
        ReactHandler *handler = iter->second;
        assert(nullptr != handler);
        if (0 != (handler->_enabled_events & ReactHandler::ACCEPT_MASK))
            handler->handle_accept_ready();
        else
            handler->handle_read_ready();
    }
    for (unsigned i = 0; i < write_set.fd_count; ++i)
    {
        const socket_t fd = write_set.fd_array[i];
        std::unordered_map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
        if (iter == _socket_to_handler.end())
            continue;
        ReactHandler *handler = iter->second;
        assert(nullptr != handler);
        if (0 != (handler->_enabled_events & ReactHandler::CONNECT_MASK))
            handler->handle_connect_ready();
        else
            handler->handle_write_ready();
    }
    for (unsigned i = 0; i < except_set.fd_count; ++i)
    {
        const socket_t fd = write_set.fd_array[i];
        std::unordered_map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
        if (iter == _socket_to_handler.end())
            continue;
        ReactHandler *handler = iter->second;
        assert(nullptr != handler);
        handler->handle_io_error(SockOperation::get_last_error(fd));
    }
#elif NUT_PLATFORM_OS_WINDOWS
    const INT timeout = timeout_ms;
    _poll_stage = PollStage::PollingWait;
    const int rs = ::WSAPoll(_pollfds, _size, timeout);
    _poll_stage = PollStage::HandlingEvents;
    if (SOCKET_ERROR == rs)
    {
        LOOFAH_LOG_ERRNO(WSAPoll);
        return -1;
    }
    size_t found = 0;
    for (ssize_t i = 0; i < (ssize_t) _size && found < rs; ++i)
    {
        const SHORT revents = _pollfds[i].revents;
        if (0 == revents)
            continue;

        _pollfds[i].revents = 0;
        ++found;
        ReactHandler *handler = _handlers[i];

        /**
         * POLLERR        错误
         * POLLNVAL       无效的 socket fd 被使用
         * POLLHUP        面向连接的 socket 被断开或者放弃
         * POLLPRI        Not used by Microsoft Winsock
         * POLLRDBAND     Priority band (out-of-band) 数据可读
         * POLLRDNORM     常规数据可读
         * POLLWRNORM     常规数据可写
         *
         * POLLIN = POLLRDNORM | POLLRDBAND
         * POLLOUT = POLLWRNORM
         */
        if (0 != (revents & POLLNVAL))
        {
            handler->handle_io_error(LOOFAH_ERR_INVALID_FD);
        }
        else if (0 != (revents & POLLERR))
        {
            handler->handle_io_error(LOOFAH_ERR_UNKNOWN);
        }
        else
        {
            if (0 != (revents & POLLHUP) || 0 != (revents & POLLIN))
            {
                if (0 != (handler->_enabled_events & ReactHandler::ACCEPT_MASK))
                    handler->handle_accept_ready();
                else
                    handler->handle_read_ready();
            }
            if (0 != (revents & POLLOUT))
            {
                if (0 != (handler->_enabled_events & ReactHandler::CONNECT_MASK))
                    handler->handle_connect_ready();
                else
                    handler->handle_write_ready();
            }
        }

        // 如果结构发生改变，则重新定位搜索位置
        if (i >= _size || _handlers[i] != handler)
        {
            i = binary_search(handler);
            if (i < 0)
                i = -i - 2; // NOTE 'i' 可能取到 -1
        }
    }
#elif NUT_PLATFORM_OS_MACOS
    struct timespec timeout;
    if (timeout_ms < 0)
    {
        timeout.tv_sec = 31536000; // NOTE 太大的数会溢出导致无法正常工作timeout.tv_sec = 3
        timeout.tv_nsec = 999999999;
    }
    else
    {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    }
    struct kevent active_evs[LOOFAH_MAX_ACTIVE_EVENTS];

    _poll_stage = PollStage::PollingWait;
    const int n = ::kevent(_kq, nullptr, 0, active_evs, LOOFAH_MAX_ACTIVE_EVENTS, &timeout);
    _poll_stage = PollStage::HandlingEvents;
    for (int i = 0; i < n; ++i)
    {
        // User event
        const int filter = active_evs[i].filter;
        if (EVFILT_USER == filter)
        {
            assert(KQUEUE_WAKEUP_IDENT == active_evs[i].ident &&
                   nullptr == active_evs[i].udata);
            continue;
        }

        // Socket events
        ReactHandler *handler = (ReactHandler*) active_evs[i].udata;
        assert(nullptr != handler);

        if (EVFILT_READ == filter)
        {
            if (0 != (handler->_enabled_events & ReactHandler::ACCEPT_MASK))
                handler->handle_accept_ready();
            else
                handler->handle_read_ready();
        }
        else if (EVFILT_WRITE == filter)
        {
            if (0 != (handler->_enabled_events & ReactHandler::CONNECT_MASK))
                handler->handle_connect_ready();
            else
                handler->handle_write_ready();
        }
        else
        {
            NUT_LOG_E(TAG, "unknown kevent filter type %d", filter);
            return -1;
        }
    }
#elif NUT_PLATFORM_OS_LINUX
    const int timeout = (timeout_ms < 0 ? -1 : timeout_ms);
    struct epoll_event events[LOOFAH_MAX_ACTIVE_EVENTS];
    _poll_stage = PollStage::PollingWait;
    const int n = ::epoll_wait(_epoll_fd, events, LOOFAH_MAX_ACTIVE_EVENTS, timeout);
    _poll_stage = PollStage::HandlingEvents;
    if (n < 0)
    {
        LOOFAH_LOG_ERRNO(epoll_wait);
        return -1;
    }

    for (int i = 0; i < n; ++i)
    {
        // 'eventfd' events
        if (events[i].data.fd == _event_fd)
        {
            if (0 != (events[i].events & EPOLLERR))
            {
                NUT_LOG_W(TAG, "eventfd error, fd %d", _event_fd);
            }
            else if (0 != (events[i].events & EPOLLIN))
            {
                uint64_t counter = 0;
                const int rs = ::read(_event_fd, &counter, sizeof(counter));
                if (rs < 0)
                    LOOFAH_LOG_FD_ERRNO(read, _event_fd);
            }

            continue;
        }

        // Socket events
        ReactHandler *handler = (ReactHandler*) events[i].data.ptr;
        if (0 != (events[i].events & EPOLLERR))
        {
            const int fd = handler->get_socket();
            const int errcode = SockOperation::get_last_error(fd);
            handler->handle_io_error(from_errno(errcode));
            continue;
        }

        // NOTE 可能既有 EPOLLIN 事件, 又有 EPOLLOUT 事件
        if (0 != (events[i].events & EPOLLIN))
        {
            if (0 != (handler->_enabled_events & ReactHandler::ACCEPT_MASK))
                handler->handle_accept_ready();
            else
                handler->handle_read_ready();
        }
        if (0 != (events[i].events & EPOLLOUT))
        {
            if (0 != (handler->_enabled_events & ReactHandler::CONNECT_MASK))
                handler->handle_connect_ready();
            else
                handler->handle_write_ready();
        }
    }
#endif

    // Run asynchronized tasks
    _poll_stage = PollStage::NotPolling;
    run_later_tasks();

    return 0;
}

void Reactor::wakeup_poll_wait()
{
#if NUT_PLATFORM_OS_MACOS
    struct kevent ev;
    EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
        LOOFAH_LOG_FD_ERRNO(kevent, _kq);
#elif NUT_PLATFORM_OS_LINUX
    const uint64_t counter = 1;
    const int rs = ::write(_event_fd, &counter, sizeof(counter));
    if (rs < 0)
        LOOFAH_LOG_FD_ERRNO(write, _event_fd);
#endif
}

}
