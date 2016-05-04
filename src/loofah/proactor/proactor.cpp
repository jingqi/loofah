
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_MAC
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#endif

#include <nut/logging/logger.h>

#include "proactor.h"

#define TAG "loofah.proactor"

namespace loofah
{

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
#elif NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
#endif
}

void Proactor::handle_events(int timeout_ms)
{
#if NUT_PLATFORM_OS_WINDOWS
    DWORD bytes_transfered = 0;
    void *key = NULL;
    OVERLAPPED *io_overlapped = NULL;
    BOOL rs = ::GetQueuedCompletionStatus(_iocp, &bytes_transfered, (PULONG_PTR)&key, &io_overlapped, timeout_ms);
    if (FALSE == rs)
    {
        NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus()");
        return;
    }

    assert(NULL != io_overlapped);
    struct IOContext *io_context = CONTAINING_RECORD(io_overlapped, struct IOContext, overlapped);
    assert(NULL != io_context && NULL != io_context->handler);
    switch (io_context->mask)
    {
    case AsyncEventHandler::ACCEPT_MASK:
        io_context->handler->handle_accept_completed(io_context);
        break;

    case AsyncEventHandler::READ_MASK:
        io_context->handler->handle_read_completed(io_context);
        break;

    case AsyncEventHandler::WRITE_MASK:
        io_context->handler->handle_write_completed(io_context);
        break;

    default:
        NUT_LOG_E(TAG, "unknown mask %d", io_context->mask);
    }
#endif
}

}
