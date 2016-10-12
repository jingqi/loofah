/**
 * 关于 Windows 下的完成端口，参加 http://blog.csdn.net/piggyxp/article/details/6922277
 */

#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#include <nut/logging/logger.h>

#include "proactor.h"
#include "../base/utils.h"

#define TAG "loofah.proactor"

namespace loofah
{

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (NULL == _iocp)
        NUT_LOG_E(TAG, "failed to create IOCP handle with errno %d", ::GetLastError());
#endif
}

Proactor::~Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    if (NULL != _iocp)
        ::CloseHandle(_iocp);
    _iocp = NULL;
#endif
}

void Proactor::register_handler(AsyncEventHandler *handler)
{
    assert(NULL != handler);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    assert(NULL != _iocp);
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
    // Create socket
    socket_t accept_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    //socket_t accept_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == accept_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return;
    }

    // Call ::AcceptEx()
    IOContext *io_context = (IOContext*) ::malloc(sizeof(IOContext));
    assert(NULL != io_context);
    new (io_context) IOContext(AsyncEventHandler::ACCEPT_MASK,
                               2 * (sizeof(struct sockaddr_in) + 16),
                               accept_socket);
    DWORD bytes = 0;
    assert(NULL != func_AcceptEx);
    const BOOL rs = func_AcceptEx(listen_socket,
                                  accept_socket,
                                  io_context->wsabuf.buf,
                                  io_context->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16), // substract address length
                                  sizeof(struct sockaddr_in) + 16, // local address length
                                  sizeof(struct sockaddr_in) + 16, // remote address length
                                  &bytes,
                                  &io_context->overlapped);
    if (FALSE == rs)
    {
        const int errcode = ::WSAGetLastError();
        // NOTE ERROR_IO_PENDING 说明异步调用没有可立即处理的数据，属于正常情况
        if (ERROR_IO_PENDING != errcode)
        {
            NUT_LOG_E(TAG, "failed to call ::AcceptEx() with errno %d", errcode);
            ::closesocket(accept_socket);
            return;
        }
    }
#endif
}

void Proactor::launch_read(AsyncEventHandler *handler, int buf_len)
{
    assert(NULL != handler);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    IOContext *io_context = (IOContext*) ::malloc(sizeof(IOContext));
    assert(NULL != io_context);
    new (io_context) IOContext(AsyncEventHandler::READ_MASK, buf_len);
    DWORD bytes = 0, flags = 0;
    const int rs = ::WSARecv(fd,
                             &io_context->wsabuf,
                             1, // wsabuf 的数量
                             &bytes, // 如果接收操作立即完成，这里会返回函数调用所接收到的字节数
                             &flags, // FIXME 貌似这里设置为 NULL 会导致错误
                             &io_context->overlapped,
                             NULL);
    if (SOCKET_ERROR == rs)
    {
        const int errcode = ::WSAGetLastError();
        if (ERROR_IO_PENDING != errcode)
        {
            NUT_LOG_E(TAG, "failed to call ::WSARecv() with errno %d", errcode);
            return;
        }
    }
#endif
}

void Proactor::launch_write(AsyncEventHandler *handler, const void *buf, int buf_len)
{
    assert(NULL != buf && buf_len > 0);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    IOContext *io_context = (IOContext*) ::malloc(sizeof(IOContext));
    assert(NULL != io_context);
    new (io_context) IOContext(AsyncEventHandler::WRITE_MASK, buf_len);
    ::memcpy(io_context->wsabuf.buf, buf, buf_len); // TODO optimize this
    DWORD bytes = 0;
    const int rs = ::WSASend(fd,
                             &io_context->wsabuf,
                             1, // wsabuf 的数量
                             &bytes, // 如果发送操作立即完成，这里会返回函数调用所发送的字节数
                             0,
                             &io_context->overlapped,
                             NULL);
    if (SOCKET_ERROR == rs)
    {
        const int errcode = ::WSAGetLastError();
        if (ERROR_IO_PENDING != errcode)
        {
            NUT_LOG_E(TAG, "failed to call ::WSASend() with errno %d", errcode);
            return;
        }
    }
#endif
}

void Proactor::handle_events(int timeout_ms)
{
#if NUT_PLATFORM_OS_WINDOWS
    DWORD bytes_transfered = 0;
    void *key = NULL;
    OVERLAPPED *io_overlapped = NULL;
    BOOL rs = ::GetQueuedCompletionStatus(_iocp, &bytes_transfered, (PULONG_PTR)&key,
                                          &io_overlapped, timeout_ms);
    if (FALSE == rs)
    {
        const DWORD errcode = ::GetLastError();
        // NOTE WAIT_TIMEOUT 表示等待超时
        if (WAIT_TIMEOUT != errcode)
        {
            NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus() with "
                      "errno %d", errcode);
        }
        return;
    }

    AsyncEventHandler *handler = (AsyncEventHandler*) key;
    assert(NULL != handler);

    assert(NULL != io_overlapped);
    IOContext *io_context = CONTAINING_RECORD(io_overlapped, IOContext, overlapped);
    assert(NULL != io_context);

    switch (io_context->event_type)
    {
    case AsyncEventHandler::ACCEPT_MASK:
        handler->handle_accept_completed(io_context);
        break;

    case AsyncEventHandler::READ_MASK:
        handler->handle_read_completed(bytes_transfered, io_context);
        break;

    case AsyncEventHandler::WRITE_MASK:
        handler->handle_write_completed(bytes_transfered, io_context);
        break;

    default:
        NUT_LOG_E(TAG, "unknown event type %d", io_context->event_type);
    }
    io_context->~IOContext();
    ::free(io_context);
#endif
}

}
