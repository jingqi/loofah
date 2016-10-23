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
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h>
#   include <unistd.h> // for ::close()
#endif

#include <string.h>

#include <nut/logging/logger.h>
#include <nut/threading/sync/guard.h>

#include "reactor.h"


#define TAG "loofah.reactor"
#define MAX_EPOLL_EVENTS 32
#define MAX_KQUEUE_EVENTS 32

namespace loofah
{

Reactor::Reactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
#elif NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
    if (-1 == _kq)
    {
        NUT_LOG_E(TAG, "failed to call ::kqueue() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    _epoll_fd = ::epoll_create(MAX_EPOLL_EVENTS);
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
}

void Reactor::shutdown()
{
#if NUT_PLATFORM_OS_WINDOWS
    nut::Guard<nut::Mutex> g(&_mutex);
    FD_ZERO(&_read_set);
    FD_ZERO(&_write_set);
    FD_ZERO(&_except_set);
    _socket_to_handler.clear();
#elif NUT_PLATFORM_OS_MAC
    if (-1 != _kq)
        ::close(_kq);
    _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    if (-1 != _epoll_fd)
        ::close(_epoll_fd);
    _epoll_fd = -1;
#endif

    _closed = true;
}

void Reactor::register_handler(ReactHandler *handler, int mask)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    nut::Guard<nut::Mutex> g(&_mutex);
    if (0 != (mask & ReactHandler::READ_MASK))
        FD_SET(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        FD_SET(fd, &_write_set);
    _socket_to_handler.insert(std::pair<socket_t,ReactHandler*>(fd, handler));
#elif NUT_PLATFORM_OS_MAC
    assert(0 == handler->_registered_events);
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
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

void Reactor::unregister_handler(ReactHandler *handler)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    nut::Guard<nut::Mutex> g(&_mutex);
    if (FD_ISSET(fd, &_read_set))
        FD_CLR(fd, &_read_set);
    if (FD_ISSET(fd, &_write_set))
        FD_CLR(fd, &_write_set);
    if (FD_ISSET(fd, &_except_set))
        FD_CLR(fd, &_except_set);
    _socket_to_handler.erase(fd);
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    EV_SET(ev + 0, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
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

void Reactor::enable_handler(ReactHandler *handler, int mask)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    nut::Guard<nut::Mutex> g(&_mutex);
    if (0 != (mask & ReactHandler::READ_MASK) && !FD_ISSET(fd, &_read_set))
        FD_SET(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK) && !FD_ISSET(fd, &_write_set))
        FD_SET(fd, &_write_set);
    _socket_to_handler.insert(std::pair<socket_t,ReactHandler*>(fd, handler));
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
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
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

void Reactor::disable_handler(ReactHandler *handler, int mask)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    nut::Guard<nut::Mutex> g(&_mutex);
    if (0 != (mask & ReactHandler::READ_MASK) && FD_ISSET(fd, &_read_set))
        FD_CLR(fd, &_read_set);
    if (0 != (mask & ReactHandler::WRITE_MASK) && FD_ISSET(fd, &_write_set))
        FD_CLR(fd, &_write_set);
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ReactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (mask & ReactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
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
    if (_closed)
        return -1;

#if NUT_PLATFORM_OS_WINDOWS
    struct timeval timeout, *ptimeout = NULL; // NULL 表示无限等待
    if (timeout_ms >= 0)
    {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        ptimeout = &timeout;
    }

    _mutex.lock();
    FD_SET read_set = _read_set, write_set = _write_set, except_set = _except_set;
    _mutex.unlock();

    const int rs = ::select(0, &read_set, &write_set, &except_set, ptimeout);
    if (SOCKET_ERROR == rs)
    {
        NUT_LOG_E(TAG, "failed to call ::select() with errorno %d", ::WSAGetLastError());
        return -1;
    }
    for (unsigned i = 0; i < read_set.fd_count; ++i)
    {
        socket_t fd = read_set.fd_array[i];
        std::map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
        if (iter == _socket_to_handler.end())
            continue;
        ReactHandler *handler = iter->second;
        assert(NULL != handler);
        handler->handle_read_ready();
    }
    for (unsigned i = 0; i < write_set.fd_count; ++i)
    {
        socket_t fd = write_set.fd_array[i];
        std::map<socket_t, ReactHandler*>::const_iterator iter = _socket_to_handler.find(fd);
        if (iter == _socket_to_handler.end())
            continue;
        ReactHandler *handler = iter->second;
        assert(NULL != handler);
        handler->handle_write_ready();
    }
    return 0;
#elif NUT_PLATFORM_OS_MAC
    struct timespec timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    struct kevent active_evs[MAX_KQUEUE_EVENTS];

    int n = ::kevent(_kq, NULL, 0, active_evs, MAX_KQUEUE_EVENTS, &timeout);
    for (int i = 0; i < n; ++i)
    {
        ReactHandler *handler = (ReactHandler*) active_evs[i].udata;
        assert(NULL != handler);

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
    return 0;
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event events[MAX_EPOLL_EVENTS];
    const int n = ::epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, timeout_ms);
    for (int i = 0; i < n; ++i)
    {
        ReactHandler *handler = (ReactHandler*) events[i].data.ptr;
        assert(NULL != handler);

        if (0 != (events[i].events & EPOLLIN))
            handler->handle_read_ready();
        if (0 != (events[i].events & EPOLLOUT))
            handler->handle_write_ready();
    }
    return 0;
#endif
}

}
