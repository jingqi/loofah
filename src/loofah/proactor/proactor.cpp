
#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_MAC
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#endif

#include <nut/logging/logger.h>

#include "proactor.h"
#include "../utils.h"

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

Proactor::~Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    ::CloseHandle(_iocp);
#endif
}

void Proactor::register_handler(AsyncEventHandler *handler)
{
    assert(NULL != handler);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    const HANDLE rs_iocp = ::CreateIoCompletionPort((HANDLE)fd, _iocp, (ULONG_PTR) handler, 0);
    if (NULL == rs_iocp)
    {
        NUT_LOG_E(TAG, "failed to associate IOCP with errno %d", ::GetLastError());
        return;
    }
#endif
}

void Proactor::launch_accept(AsyncEventHandler *handler)
{
    assert(NULL != handler);
    socket_t listen_socket = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    // create socket
    socket_t accept_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == accept_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return;
    }

    // call ::AcceptEx()
    struct IOContext *io_context = (struct IOContext*) ::malloc(sizeof(struct IOContext));
    assert(NULL != io_context);
    new (io_context) struct IOContext(AsyncEventHandler::ACCEPT_MASK, 2 * (sizeof(struct sockaddr_in) + 16), accept_socket);
    DWORD bytes = 0;
    assert(NULL != func_AcceptEx);
    const BOOL rs = func_AcceptEx(listen_socket,
                                  accept_socket,
                                  io_context->wsabuf.buf,
                                  io_context->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16), // substract address length
                                  sizeof(struct sockaddr_in) + 16, // local address length
                                  sizeof(struct sockaddr_in) + 16, // remote address length
                                  &bytes,
                                  (LPOVERLAPPED)io_context);
    if (FALSE == rs)
    {
        NUT_LOG_E(TAG, "failed to call ::AcceptEx()");
        ::closesocket(accept_socket);
        return;
    }
#endif
}

void Proactor::launch_read(AsyncEventHandler *handler, int buf_len)
{
    assert(NULL != handler && buf_len > 0);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    struct IOContext *io_context = (struct IOContext*) ::malloc(sizeof(struct IOContext));
    assert(NULL != io_context);
    new (io_context) struct IOContext(AsyncEventHandler::READ_MASK, buf_len);
    DWORD bytes = 0;
    ::WSARecv(fd, &io_context->wsabuf, 1, &bytes, 0, (LPOVERLAPPED)io_context, NULL);
#endif
}

void Proactor::launch_write(AsyncEventHandler *handler, const void *buf, int buf_len)
{
    assert(NULL != buf && buf_len > 0);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    struct IOContext *io_context = (struct IOContext*) ::malloc(sizeof(struct IOContext));
    assert(NULL != io_context);
    new (io_context) struct IOContext(AsyncEventHandler::WRITE_MASK, buf_len);
    ::memcpy(io_context->wsabuf.buf, buf, buf_len); // TODO optimize this
    DWORD bytes = 0;
    ::WSASend(fd, &io_context->wsabuf, 1, &bytes, 0, (LPOVERLAPPED)io_context, NULL);
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

    AsyncEventHandler *handler = (AsyncEventHandler*) key;
    assert(NULL != handler);

    assert(NULL != io_overlapped);
    struct IOContext *io_context = CONTAINING_RECORD(io_overlapped, struct IOContext, overlapped);
    assert(NULL != io_context);

    switch (io_context->event_type)
    {
    case AsyncEventHandler::ACCEPT_MASK:
        handler->handle_accept_completed(io_context);
        break;

    case AsyncEventHandler::READ_MASK:
        handler->handle_read_completed(io_context);
        break;

    case AsyncEventHandler::WRITE_MASK:
        handler->handle_write_completed(io_context);
        break;

    default:
        NUT_LOG_E(TAG, "unknown event type %d", io_context->event_type);
    }
#endif
}

}
