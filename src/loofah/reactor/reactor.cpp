
#include <assert.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h>
#elif NUT_PLATFORM_OS_MAC
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#   include <unistd.h> // for close()
#endif

#include <string.h>

#include <nut/logging/logger.h>

#include "reactor.h"

#define TAG "loofah.reactor"
#define MAX_EPOLL_EVENTS 1024

namespace loofah
{

Reactor::Reactor()
{
#if NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
#elif NUT_PLATFORM_OS_LINUX
	_epoll_fd = ::epoll_create(MAX_EPOLL_EVENTS);
#endif
}

Reactor::~Reactor()
{
#if NUT_PLATFORM_OS_MAC
	::close(_kq);
#elif NUT_PLATFORM_OS_LINUX
	::close(_epoll_fd);
#endif
}

void Reactor::register_handler(SyncEventHandler *handler, int mask)
{
    assert(NULL != handler && 0 == handler->_registered_events);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & SyncEventHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != (mask & SyncEventHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
	struct epoll_event epv;
	::memset(&epv, 0, sizeof(epv));
	epv.data.ptr = (void*) handler;
	if (0 != (mask & SyncEventHandler::READ_MASK))
		epv.events |= EPOLLIN;
	if (0 != (mask & SyncEventHandler::WRITE_MASK))
		epv.events |= EPOLLOUT;
	if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &epv))
	{
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
	}
#endif

	handler->_registered_events = mask;
}

void Reactor::unregister_handler(SyncEventHandler *handler)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    EV_SET(ev + 0, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
	struct epoll_event epv;
	::memset(&epv, 0, sizeof(epv));
	epv.data.ptr = (void*) handler;
	if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &epv))
	{
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
	}
#endif

	handler->_registered_events = 0;
}

void Reactor::enable_handler(SyncEventHandler *handler, int mask)
{
	assert(NULL != handler);
	const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
	struct kevent ev[2];
	int n = 0;
	if (0 != (mask & SyncEventHandler::READ_MASK))
	{
		if (0 != (handler->_registered_events & SyncEventHandler::READ_MASK))
			EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*) handler);
		else
			EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
	}
	if (0 != (mask & SyncEventHandler::WRITE_MASK))
	{
		if (0 != (handler->_registered_events & SyncEventHandler::WRITE_MASK))
			EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ENABLE, 0, 0, (void*) handler);
		else
			EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
	}
	if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
	{
		NUT_LOG_E(TAG, "failed to call ::kevent()");
		return;
	}
#elif NUT_PLATFORM_OS_LINUX
	struct epoll_event epv;
	::memset(&epv, 0, sizeof(epv));
	epv.data.ptr = (void*) handler;
	if (0 != (mask & SyncEventHandler::READ_MASK) || 0 != (handler->_registered_events & SyncEventHandler::READ_MASK))
		epv.events |= EPOLLIN;
	if (0 != (mask & SyncEventHandler::WRITE_MASK) || 0 != (handler->_registered_events & SyncEventHandler::WRITE_MASK))
		epv.events |= EPOLLOUT;
	if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
	{
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
	}
#endif

	handler->_registered_events |= mask;
}

void Reactor::disable_handler(SyncEventHandler *handler, int mask)
{
	assert(NULL != handler);
	const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
	struct kevent ev[2];
	int n = 0;
	if (0 != (mask & SyncEventHandler::READ_MASK))
		EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
	if (0 != (mask & SyncEventHandler::WRITE_MASK))
		EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
	if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
	{
		NUT_LOG_E(TAG, "failed to call ::kevent()");
		return;
	}
#elif NUT_PLATFORM_OS_LINUX
	struct epoll_event epv;
	::memset(&epv, 0, sizeof(epv));
	epv.data.ptr = (void*) handler;
	if (0 == (mask & SyncEventHandler::READ_MASK) && 0 != (handler->_registered_events & SyncEventHandler::READ_MASK))
		epv.events |= EPOLLIN;
	if (0 == (mask & SyncEventHandler::WRITE_MASK) && 0 != (handler->_registered_events & SyncEventHandler::WRITE_MASK))
		epv.events |= EPOLLOUT;
	if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
	{
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
	}
#endif
}

void Reactor::handle_events(int timeout_ms)
{
#if NUT_PLATFORM_OS_MAC
    struct timespec timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    const int max_events = 20;
    struct kevent active_evs[max_events];

    int n = ::kevent(_kq, NULL, 0, active_evs, max_events, &timeout);
    for (int i = 0; i < n; ++i)
    {
        SyncEventHandler *handler = (SyncEventHandler*) active_evs[i].udata;
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
            return;
        }
    }
#elif NUT_PLATFORM_OS_LINUX
	struct epoll_event events[MAX_EPOLL_EVENTS];
	int n = ::epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, timeout_ms);
	for (int i = 0; i < n; ++i)
	{
		SyncEventHandler *handler = (SyncEventHandler*) events[i].data.ptr;
		assert(NULL != handler);

		if (0 != (events[i].events & EPOLLIN))
			handler->handle_read_ready();
		if (0 != (events[i].events & EPOLLOUT))
			handler->handle_write_ready();
	}
#endif
}

}
