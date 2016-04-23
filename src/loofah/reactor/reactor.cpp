
#include <assert.h>

#include <nut/platform/platform.h>

#if defined(NUT_PLATFORM_OS_MAC)
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#endif

#include <nut/logging/logger.h>

#include "reactor.h"

#define TAG "loofah.reactor"

namespace loofah
{

Reactor::Reactor()
{
#if defined(NUT_PLATFORM_OS_MAC)
    _kq = ::kqueue();
#endif
}

void Reactor::register_handler(SyncEventHandler *handler, int mask)
{
    assert(NULL != handler);
    const handle_t fd = handler->get_handle();

#if defined(NUT_PLATFORM_OS_MAC)
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & SyncEventHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*)handler);
    if (0 != (mask & SyncEventHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*)handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
        return;
    }
#endif
}

void Reactor::unregister_handler(SyncEventHandler *handler, int mask)
{
    assert(NULL != handler);
    const handle_t fd = handler->get_handle();

#if defined(NUT_PLATFORM_OS_MAC)
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & SyncEventHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*)handler);
    if (0 != (mask & SyncEventHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*)handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent()");
        return;
    }
#endif
}

void Reactor::enable_handler(SyncEventHandler *handler, int mask)
{
	assert(NULL != handler);
	const handle_t fd = handler->get_handle();

#if defined(NUT_PLATFORM_OS_MAC)
	struct kevent ev[2];
	int n = 0;
	if (0 != (mask & SyncEventHandler::READ_MASK))
		EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*)handler);
	if (0 != (mask & SyncEventHandler::WRITE_MASK))
		EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ENABLE, 0, 0, (void*)handler);
	if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
	{
		NUT_LOG_E(TAG, "failed to call ::kevent()");
		return;
	}
#endif
}

void Reactor::disable_handler(SyncEventHandler *handler, int mask)
{
	assert(NULL != handler);
	const handle_t fd = handler->get_handle();

#if defined(NUT_PLATFORM_OS_MAC)
	struct kevent ev[2];
	int n = 0;
	if (0 != (mask & SyncEventHandler::READ_MASK))
		EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*)handler);
	if (0 != (mask & SyncEventHandler::WRITE_MASK))
		EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*)handler);
	if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
	{
		NUT_LOG_E(TAG, "failed to call ::kevent()");
		return;
	}
#endif
}

void Reactor::handle_events(int timeout_ms)
{
#if defined(NUT_PLATFORM_OS_MAC)
    struct timespec timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    const int max_events = 20;
    struct kevent active_evs[max_events];

    int n = ::kevent(_kq, NULL, 0, active_evs, max_events, &timeout);
    for (int i = 0; i < n; ++i)
    {
        SyncEventHandler *handler = (SyncEventHandler*)(intptr_t) active_evs[i].udata;
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
#endif
}

}
