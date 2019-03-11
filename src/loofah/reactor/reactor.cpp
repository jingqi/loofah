﻿
#include <assert.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#elif NUT_PLATFORM_OS_MAC
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#endif

#include <string.h>

#include <nut/logging/logger.h>
#include <nut/rc/rc_new.h>

#include "reactor.h"


#define TAG "loofah.reactor"

namespace loofah
{

Reactor::Reactor()
{
#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
#   else
    _pollfds = (WSAPOLLFD*) ::malloc(sizeof(WSAPOLLFD) * _capacity);
    _handlers = (ReactHandler**) ::malloc(sizeof(ReactHandler*) * _capacity);
#   endif
#elif NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
    if (-1 == _kq)
    {
        NUT_LOG_E(TAG, "failed to call ::kqueue() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    // NOTE 自从 Linux2.6.8 版本以后，epoll_create() 参数值其实是没什么用的, 只
    //      需要大于 0
    _epoll_fd = ::epoll_create(2048);
    if (-1 == _epoll_fd)
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_create()");
        return;
    }
#endif
}

Reactor::~Reactor()
{
    shutdown();

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER >= _WIN32_WINNT_WINBLUE
    if (nullptr != _pollfds)
        ::free(_pollfds);
    _pollfds = nullptr;
    if (nullptr != _handlers)
        ::free(_handlers);
    _handlers = nullptr;
#   endif
#endif
}

void Reactor::shutdown_later()
{
    _closing_or_closed = true;
}

void Reactor::shutdown()
{
    assert(is_in_loop_thread());

    _closing_or_closed = true;

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
    _socket_to_handler.clear();
#   else
    _size = 0;
#   endif
#elif NUT_PLATFORM_OS_MAC
    if (-1 != _kq)
        ::close(_kq);
    _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    if (-1 != _epoll_fd)
        ::close(_epoll_fd);
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

size_t Reactor::index_of(ReactHandler *handler)
{
    for (size_t i = 0; i < _size; ++i)
    {
        if (_handlers[i] == handler)
            return i;
    }
    return _size;
}
#endif

void Reactor::register_handler_later(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
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

void Reactor::register_handler(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    if (0 != (mask & ReactHandler::READ_MASK))
        FD_SET(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        FD_SET(fd, &_write_set);
    _socket_to_handler.insert(std::pair<socket_t,ReactHandler*>(fd, handler));
#   else
    ensure_capacity(_size + 1);
    _handlers[_size] = handler;
    _pollfds[_size].fd = fd;
    _pollfds[_size].events = 0;
    _pollfds[_size].revents = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
        _pollfds[_size].events |= POLLIN;
    if (0 != (mask & ReactHandler::WRITE_MASK))
        _pollfds[_size].events |= POLLOUT;
    ++_size;
#   endif
#elif NUT_PLATFORM_OS_MAC
    assert(0 == handler->_registered_events);
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
    handler->_registered_events = mask;
#elif NUT_PLATFORM_OS_LINUX
    assert(0 == handler->_registered_events);
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 != (mask & ReactHandler::READ_MASK))
        epv.events |= EPOLLIN;
    if (0 != (mask & ReactHandler::WRITE_MASK))
        epv.events |= EPOLLOUT;
    if (_edge_triggered)
        epv.events |= EPOLLET;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
    handler->_registered_events = mask;
#endif
}

void Reactor::unregister_handler_later(ReactHandler *handler)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
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
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    if (FD_ISSET(fd, &_read_set))
        FD_CLR(fd, &_read_set);
    if (FD_ISSET(fd, &_write_set))
        FD_CLR(fd, &_write_set);
    if (FD_ISSET(fd, &_except_set))
        FD_CLR(fd, &_except_set);
    _socket_to_handler.erase(fd);
#   else
    const size_t index = index_of(handler);
    if (index >= _size)
        return;
    if (index + 1 < _size)
    {
        // Swap current and last element
        _pollfds[index] = _pollfds[_size - 1];
        _handlers[index] = _handlers[_size - 1];
    }
    --_size;
#   endif
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    EV_SET(ev, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, nullptr, 0, nullptr))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
    handler->_registered_events = 0;
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
    handler->_registered_events = 0;
#endif
}

void Reactor::enable_handler_later(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
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

void Reactor::enable_handler(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    if (0 != (mask & ReactHandler::READ_MASK) && !FD_ISSET(fd, &_read_set))
        FD_SET(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK) && !FD_ISSET(fd, &_write_set))
        FD_SET(fd, &_write_set);
    _socket_to_handler.insert(std::pair<socket_t,ReactHandler*>(fd, handler));
#   else
    const size_t index = index_of(handler);
    if (index >= _size)
        return;
    if (0 != (mask & ReactHandler::READ_MASK))
        _pollfds[index].events |= POLLIN;
    if (0 != (mask & ReactHandler::WRITE_MASK))
        _pollfds[index].events |= POLLOUT;
#   endif
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
    {
        if (0 != (handler->_registered_events & ReactHandler::READ_MASK))
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (0 != (mask & ReactHandler::WRITE_MASK))
    {
        if (0 != (handler->_registered_events & ReactHandler::WRITE_MASK))
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
    handler->_registered_events |= mask;
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 != (mask & ReactHandler::READ_MASK) || 0 != (handler->_registered_events & ReactHandler::READ_MASK))
        epv.events |= EPOLLIN;
    if (0 != (mask & ReactHandler::WRITE_MASK) || 0 != (handler->_registered_events & ReactHandler::WRITE_MASK))
        epv.events |= EPOLLOUT;
    if (_edge_triggered)
        epv.events |= EPOLLET;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
    handler->_registered_events |= mask;
#endif
}

void Reactor::disable_handler_later(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
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

void Reactor::disable_handler(ReactHandler *handler, int mask)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
    if (0 != (mask & ReactHandler::READ_MASK) && FD_ISSET(fd, &_read_set))
        FD_CLR(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK) && FD_ISSET(fd, &_write_set))
        FD_CLR(fd, &_write_set);
#   else
    const size_t index = index_of(handler);
    if (index >= _size)
        return;
    if (0 != (mask & ReactHandler::READ_MASK))
        _pollfds[index].events &= ~POLLIN;
    if (0 != (mask & ReactHandler::WRITE_MASK))
        _pollfds[index].events &= ~POLLOUT;
#   endif
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
    handler->_registered_events &= ~mask;
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 == (mask & ReactHandler::READ_MASK) && 0 != (handler->_registered_events & ReactHandler::READ_MASK))
        epv.events |= EPOLLIN;
    if (0 == (mask & ReactHandler::WRITE_MASK) && 0 != (handler->_registered_events & ReactHandler::WRITE_MASK))
        epv.events |= EPOLLOUT;
    if (_edge_triggered)
        epv.events |= EPOLLET;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
    handler->_registered_events &= ~mask;
#endif
}

int Reactor::handle_events(int timeout_ms)
{
    if (_closing_or_closed)
        return -1;

    if (!is_in_loop_thread())
    {
        NUT_LOG_F(TAG, "handle_events() can only be called from inside loop thread");
        return -1;
    }

    {
        HandleEventsGuard g(this);

#if NUT_PLATFORM_OS_WINDOWS
#   if WINVER < _WIN32_WINNT_WINBLUE
        struct timeval timeout, *ptimeout = nullptr; // nullptr 表示无限等待
        if (timeout_ms >= 0)
        {
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;
            ptimeout = &timeout;
        }

        FD_SET read_set = _read_set, write_set = _write_set, except_set = _except_set;

        const int rs = ::select(0, &read_set, &write_set, &except_set, ptimeout);
        if (SOCKET_ERROR == rs)
        {
            NUT_LOG_E(TAG, "failed to call ::select() with errorno %d", ::WSAGetLastError());
            return -1;
        }
        for (unsigned i = 0; i < read_set.fd_count; ++i)
        {
            socket_t fd = read_set.fd_array[i];
            std::unordered_map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
            if (iter == _socket_to_handler.end())
                continue;
            ReactHandler *handler = iter->second;
            assert(nullptr != handler);
            handler->handle_read_ready();
        }
        for (unsigned i = 0; i < write_set.fd_count; ++i)
        {
            socket_t fd = write_set.fd_array[i];
            std::unordered_map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
            if (iter == _socket_to_handler.end())
                continue;
            ReactHandler *handler = iter->second;
            assert(nullptr != handler);
            handler->handle_write_ready();
        }
#   else
        const int rs = ::WSAPoll(_pollfds, _size, timeout_ms);
        if (SOCKET_ERROR == rs)
        {
            NUT_LOG_E(TAG, "failed to call ::WSAPoll() with errorno %d", ::WSAGetLastError());
            return -1;
        }
        int found = 0;
        for (size_t i = 0; i < _size && found < rs; ++i)
        {
            const SHORT revents = _pollfds[i].revents;
            _pollfds[i].revents = 0;
            if (0 != revents)
            {
                ++found;
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
                if (0 != (revents & POLLERR) || 0 != (revents & POLLNVAL))
                {
                    NUT_LOG_E(TAG, "error detected with fd %d when calling ::WSAPoll()", _pollfds[i].fd);
                    return -1;
                }
                if (0 != (revents & POLLHUP) || 0 != (revents & POLLIN))
                    _handlers[i]->handle_read_ready();
                if (0 != (revents & POLLOUT))
                    _handlers[i]->handle_write_ready();
            }
        }
#   endif
#elif NUT_PLATFORM_OS_MAC
        struct timespec timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
        struct kevent active_evs[LOOFAH_MAX_ACTIVE_EVENTS];

        int n = ::kevent(_kq, nullptr, 0, active_evs, LOOFAH_MAX_ACTIVE_EVENTS, &timeout);
        for (int i = 0; i < n; ++i)
        {
            ReactHandler *handler = (ReactHandler*) active_evs[i].udata;
            assert(nullptr != handler);

            int events = active_evs[i].filter;
            if (events == EVFILT_READ)
            {
                handler->handle_read_ready();
            }
            else if (events == EVFILT_WRITE)
            {
                handler->handle_write_ready();
            }
            else
            {
                NUT_LOG_E(TAG, "unknown kevent type %d", events);
                return -1;
            }
        }
#elif NUT_PLATFORM_OS_LINUX
        struct epoll_event events[LOOFAH_MAX_ACTIVE_EVENTS];
        const int n = ::epoll_wait(_epoll_fd, events, LOOFAH_MAX_ACTIVE_EVENTS, timeout_ms);
        for (int i = 0; i < n; ++i)
        {
            ReactHandler *handler = (ReactHandler*) events[i].data.ptr;
            assert(nullptr != handler);

            if (0 != (events[i].events & EPOLLIN))
                handler->handle_read_ready();
            if (0 != (events[i].events & EPOLLOUT))
                handler->handle_write_ready();
        }
#endif

    }

    // Run asynchronized tasks
    run_later_tasks();

    return 0;
}

}
