/**
 * 1. 关于 Windows 下的完成端口，参见 http://blog.csdn.net/piggyxp/article/details/6922277
 * 2. 关于 Linux 下 libaio + epoll，参见 http://blog.chinaunix.net/uid-16979052-id-3840266.html
 */


#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h> // for ::epoll_create()
#   include <unistd.h> // for ::close()
#   include <errno.h>
#endif

#include <assert.h>
#include <string.h>

#include <nut/logging/logger.h>
#include <nut/threading/sync/guard.h>

#include "proactor.h"
#include "proact_acceptor.h"
#include "../base/utils.h"

#define TAG "loofah.proactor"

#define MAX_EPOLL_EVENTS 1024

namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
typedef struct _IORequest
{
    // NOTE OVERLAPPED 必须放在首位
    OVERLAPPED overlapped;

    // 事件类型
    int event_type = 0;

    socket_t accept_socket = INVALID_SOCKET_VALUE;

    WSABUF wsabuf;
    bool need_free = true;

    _IORequest(int event_type_, int buf_len, socket_t accept_socket_)
        : event_type(event_type_), accept_socket(accept_socket_), need_free(true)
    {
        assert(buf_len > 0);

        ::memset(&overlapped, 0, sizeof(overlapped));

        wsabuf.buf = (char*) ::malloc(buf_len);
        assert(NULL != wsabuf.buf);
        wsabuf.len = buf_len;
    }

    _IORequest(int event_type_, void *buf, int buf_len)
        : event_type(event_type_), need_free(false)
    {
        assert(NULL != buf && buf_len > 0);

        ::memset(&overlapped, 0, sizeof(overlapped));

        wsabuf.buf = (char*) buf;
        wsabuf.len = buf_len;
    }

    ~_IORequest()
    {
        clear();
    }

    void clear()
    {
        if (need_free && NULL != wsabuf.buf)
            ::free(wsabuf.buf);
        wsabuf.buf = NULL;
        wsabuf.len = 0;
    }
} IORequest;
#else
typedef struct _IORequest
{
    // 事件类型
    int event_type = 0;
    
    void *buf = NULL;
    int buf_len = 0;

    _IORequest(int event_type_, void *buf_, int buf_len_)
        : event_type(event_type_), buf(buf_), buf_len(buf_len_)
    {}
} IORequest;
#endif

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (NULL == _iocp)
        NUT_LOG_E(TAG, "failed to create IOCP handle with errno %d", ::GetLastError());
#elif NUT_PLATFORM_OS_LINUX
    _epoll_fd = ::epoll_create(MAX_EPOLL_EVENTS);
    if (-1 == _epoll_fd)
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_create()");
        return;
    }
#endif
}

Proactor::~Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    if (NULL != _iocp)
        ::CloseHandle(_iocp);
    _iocp = NULL;
#elif NUT_PLATFORM_OS_LINUX
    ::close(_epoll_fd);
#endif
}

void Proactor::register_handler(ProactHandler *handler)
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
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    epv.events = 0;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
#endif
}

void Proactor::launch_accept(ProactHandler *handler)
{
    assert(NULL != handler);
    socket_t listener_socket = handler->get_socket();

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
    IORequest *io_request = (IORequest*) ::malloc(sizeof(IORequest));
    assert(NULL != io_request);
    new (io_request) IORequest(ProactHandler::ACCEPT_MASK,
                               2 * (sizeof(struct sockaddr_in) + 16),
                               accept_socket);
    DWORD bytes = 0;
    assert(NULL != func_AcceptEx);
    const BOOL rs = func_AcceptEx(listener_socket,
                                  accept_socket,
                                  io_request->wsabuf.buf,
                                  io_request->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16), // substract address length
                                  sizeof(struct sockaddr_in) + 16, // local address length
                                  sizeof(struct sockaddr_in) + 16, // remote address length
                                  &bytes,
                                  &io_request->overlapped);
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
#elif NUT_PLATFORM_OS_LINUX
    nut::Guard<nut::Mutex> g(&handler->_mutex);
    ++handler->_request_accept;
    if (0 == (handler->_registered_events & EPOLLIN))
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        epv.events = EPOLLIN;
        if (0 != (handler->_registered_events & EPOLLOUT))
            epv.events |= EPOLLOUT;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, listener_socket, &epv))
        {
            NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
            return;
        }
        handler->_registered_events |= EPOLLIN;
    }
#endif
}

void Proactor::launch_read(ProactHandler *handler, void *buf, int buf_len)
{
    assert(NULL != handler && NULL != buf && buf_len > 0);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    IORequest *io_request = (IORequest*) ::malloc(sizeof(IORequest));
    assert(NULL != io_request);
    new (io_request) IORequest(ProactHandler::READ_MASK, buf, buf_len);
    DWORD bytes = 0, flags = 0;
    const int rs = ::WSARecv(fd,
                             &io_request->wsabuf,
                             1, // wsabuf 的数量
                             &bytes, // 如果接收操作立即完成，这里会返回函数调用所接收到的字节数
                             &flags, // FIXME 貌似这里设置为 NULL 会导致错误
                             &io_request->overlapped,
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
#elif NUT_PLATFORM_OS_LINUX
    nut::Guard<nut::Mutex> g(&handler->_mutex);
    IORequest *io_request = (IORequest*) ::malloc(sizeof(IORequest));
    assert(NULL != io_request);
    new (io_request) IORequest(ProactHandler::READ_MASK, buf, buf_len);
    handler->_read_queue.push(io_request);

    if (0 == (handler->_registered_events & EPOLLIN))
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        epv.events = EPOLLIN;
        if (0 != (handler->_registered_events & EPOLLOUT))
            epv.events |= EPOLLOUT;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
        {
            NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
            return;
        }
        handler->_registered_events |= EPOLLIN;
    }
#endif
}

void Proactor::launch_write(ProactHandler *handler, void *buf, int buf_len)
{
    assert(NULL != buf && buf_len > 0);
    socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    IORequest *io_request = (IORequest*) ::malloc(sizeof(IORequest));
    assert(NULL != io_request);
    new (io_request) IORequest(ProactHandler::WRITE_MASK, buf, buf_len);
    DWORD bytes = 0;
    const int rs = ::WSASend(fd,
                             &io_request->wsabuf,
                             1, // wsabuf 的数量
                             &bytes, // 如果发送操作立即完成，这里会返回函数调用所发送的字节数
                             0,
                             &io_request->overlapped,
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
#elif NUT_PLATFORM_OS_LINUX
    nut::Guard<nut::Mutex> g(&handler->_mutex);
    IORequest *io_request = (IORequest*) ::malloc(sizeof(IORequest));
    assert(NULL != io_request);
    new (io_request) IORequest(ProactHandler::WRITE_MASK, buf, buf_len);
    handler->_write_queue.push(io_request);

    if (0 == (handler->_registered_events & EPOLLOUT))
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        epv.events = EPOLLOUT;
        if (0 != (handler->_registered_events & EPOLLIN))
            epv.events |= EPOLLIN;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
        {
            NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
            return;
        }
        handler->_registered_events |= EPOLLOUT;
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

    ProactHandler *handler = (ProactHandler*) key;
    assert(NULL != handler);

    assert(NULL != io_overlapped);
    IORequest *io_request = CONTAINING_RECORD(io_overlapped, IORequest, overlapped);
    assert(NULL != io_request);

    switch (io_request->event_type)
    {
    case ProactHandler::ACCEPT_MASK:
    {
        /* Get peer address
         *
        struct sockaddr_in *remote_addr = NULL, *local_addr = NULL;
        int remote_len = sizeof(struct sockaddr_in), local_len = sizeof(struct sockaddr_in);
        assert(NULL != func_GetAcceptExSockaddrs);
        func_GetAcceptExSockaddrs(io_request->wsabuf.buf,
                                  io_request->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16),
                                  sizeof(struct sockaddr_in) + 16,
                                  sizeof(struct sockaddr_in) + 16,
                                  (LPSOCKADDR*)&local_addr,
                                  &local_len,
                                  (LPSOCKADDR*)&remote_addr,
                                  &remote_len);
         */

        handler->handle_accept_completed(io_request->accept_socket);
        break;
    }

    case ProactHandler::READ_MASK:
        handler->handle_read_completed(io_request->wsabuf.buf, bytes_transfered);
        break;

    case ProactHandler::WRITE_MASK:
        handler->handle_write_completed(io_request->wsabuf.buf, bytes_transfered);
        break;

    default:
        NUT_LOG_E(TAG, "unknown event type %d", io_request->event_type);
    }
    io_request->~IORequest();
    ::free(io_request);
#elif NUT_PLATFORM_OS_LINUX

#   define CONTAINING_RECORD(addr,type,field) ((type*)((unsigned char*)addr - (unsigned long)&((type*)0)->field))

    struct epoll_event events[MAX_EPOLL_EVENTS];
    const int n = ::epoll_wait(_epoll_fd, events, MAX_EPOLL_EVENTS, timeout_ms);
    for (int i = 0; i < n; ++i)
    {
        ProactHandler *handler = (ProactHandler*) events[i].data.ptr;
        assert(NULL != handler);
        nut::Guard<nut::Mutex> g(&handler->_mutex);
        socket_t fd = handler->get_socket();

        if (0 != (events[i].events & EPOLLIN))
        {
            if (handler->_request_accept > 0)
            {
                --handler->_request_accept;
                if (handler->_request_accept <= 0)
                {
                    struct epoll_event epv;
                    ::memset(&epv, 0, sizeof(epv));
                    epv.data.ptr = (void*) handler;
                    if (0 != (handler->_registered_events & EPOLLOUT))
                        epv.events = EPOLLOUT;
                    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
                    {
                        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
                        return;
                    }
                    handler->_registered_events &= ~EPOLLIN;
                }

                socket_t accepted = ProactAcceptorBase::handle_accept(fd);
                handler->handle_accept_completed(accepted);
            }
            else
            {
                assert(!handler->_read_queue.empty());
                IORequest *io_request = (IORequest*) handler->_read_queue.front();
                assert(NULL != io_request);
                handler->_read_queue.pop();

                if (handler->_read_queue.empty())
                {
                    struct epoll_event epv;
                    ::memset(&epv, 0, sizeof(epv));
                    epv.data.ptr = (void*) handler;
                    if (0 != (handler->_registered_events & EPOLLOUT))
                        epv.events = EPOLLOUT;
                    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
                    {
                        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
                        return;
                    }
                    handler->_registered_events &= ~EPOLLIN;
                }

                const int readed = ::read(fd, io_request->buf, io_request->buf_len);
                handler->handle_read_completed(io_request->buf, readed);

                io_request->~IORequest();
                ::free(io_request);
            }
        }
        if (0 != (events[i].events & EPOLLOUT))
        {
            assert(!handler->_write_queue.empty());
            IORequest *io_request = (IORequest*) handler->_write_queue.front();
            assert(NULL != io_request);
            handler->_write_queue.pop();

            if (handler->_write_queue.empty())
            {
                struct epoll_event epv;
                ::memset(&epv, 0, sizeof(epv));
                epv.data.ptr = (void*) handler;
                if (0 != (handler->_registered_events & EPOLLIN))
                    epv.events = EPOLLIN;
                if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
                {
                    NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
                    return;
                }
                handler->_registered_events &= ~EPOLLOUT;
            }

            const int wrote = ::write(fd, io_request->buf, io_request->buf_len);
            handler->handle_write_completed(io_request->buf, wrote);

            io_request->~IORequest();
            ::free(io_request);
        }
    }
#endif
}

}
