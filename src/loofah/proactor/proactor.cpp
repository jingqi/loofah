/**
 * 1. 关于 Windows 下的完成端口，参见 http://blog.csdn.net/piggyxp/article/details/6922277
 * 2. 关于 Linux 下 libaio + epoll，参见 http://blog.chinaunix.net/uid-16979052-id-3840266.html
 */


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
#   include <sys/epoll.h> // for ::epoll_create()
#   include <unistd.h> // for ::close()
#   include <errno.h>
#endif

#include <assert.h>
#include <string.h>

#include <nut/logging/logger.h>
#include <nut/threading/sync/guard.h>

#include "proactor.h"
#include "io_request.h"
#include "../reactor/react_acceptor.h"
#include "../inet_base/utils.h"


#define TAG "loofah.proactor"
#define MAX_ACTIVE_EVENTS 32

namespace loofah
{

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (NULL == _iocp)
        NUT_LOG_E(TAG, "failed to create IOCP handle with errno %d", ::GetLastError());
#elif NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
    if (-1 == _kq)
    {
        NUT_LOG_E(TAG, "failed to call ::kqueue() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    _epoll_fd = ::epoll_create(MAX_ACTIVE_EVENTS);
    if (-1 == _epoll_fd)
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_create()");
        return;
    }
#endif

    _loop_tid = nut::Thread::current_thread_id();
}

Proactor::~Proactor()
{
    shutdown();
}

void Proactor::shutdown()
{
    _closing_or_closed = true;

#if NUT_PLATFORM_OS_WINDOWS
    if (NULL != _iocp)
        ::CloseHandle(_iocp);
    _iocp = NULL;
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
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    EV_SET(ev, fd, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
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
    IORequest *io_request = IORequest::new_request(ProactHandler::ACCEPT_MASK,
                                                   1, accept_socket);
    assert(NULL != io_request);
    const size_t buf_len = 2 * (sizeof(struct sockaddr_in) + 16);
    void *buf = ::malloc(2 * (sizeof(struct sockaddr_in) + 16));
    assert(NULL != buf);
    io_request->set_buf(0, buf, buf_len);

    DWORD bytes = 0;
    assert(NULL != func_AcceptEx);
    const BOOL rs = func_AcceptEx(listener_socket,
                                  accept_socket,
                                  buf,
                                  buf_len - 2 * (sizeof(struct sockaddr_in) + 16), // substract address length
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
#elif NUT_PLATFORM_OS_MAC
    ++handler->_request_accept;
    if (0 == (handler->_registered_events & ProactHandler::READ_MASK))
    {
        struct kevent ev;
        EV_SET(&ev, listener_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
        if (0 != ::kevent(_kq, &ev, 1, NULL, 0, NULL))
        {
            NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                      ::strerror(errno));
            return;
        }
        handler->_registered_events |= ProactHandler::READ_MASK;
    }
#elif NUT_PLATFORM_OS_LINUX
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

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
void Proactor::enable_handler(ProactHandler *handler, int mask)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ProactHandler::READ_MASK))
    {
        if (0 != (handler->_registered_events & ProactHandler::READ_MASK))
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (0 != (mask & ProactHandler::WRITE_MASK))
    {
        if (0 != (handler->_registered_events & ProactHandler::WRITE_MASK))
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
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 != (mask & ProactHandler::READ_MASK) || 0 != (handler->_registered_events & ProactHandler::READ_MASK))
        epv.events |= EPOLLIN;
    if (0 != (mask & ProactHandler::WRITE_MASK) || 0 != (handler->_registered_events & ProactHandler::WRITE_MASK))
        epv.events |= EPOLLOUT;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
#endif

    handler->_registered_events |= mask;
}

void Proactor::disable_handler(ProactHandler *handler, int mask)
{
    assert(NULL != handler);
    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ProactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (mask & ProactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, NULL, 0, NULL))
    {
        NUT_LOG_E(TAG, "failed to call ::kevent() with errno %d: %s", errno,
                  ::strerror(errno));
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 == (mask & ProactHandler::READ_MASK) && 0 != (handler->_registered_events & ProactHandler::READ_MASK))
        epv.events |= EPOLLIN;
    if (0 == (mask & ProactHandler::WRITE_MASK) && 0 != (handler->_registered_events & ProactHandler::WRITE_MASK))
        epv.events |= EPOLLOUT;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
    {
        NUT_LOG_E(TAG, "failed to call ::epoll_ctl()");
        return;
    }
#endif

    handler->_registered_events &= ~mask;
}
#endif

void Proactor::launch_read(ProactHandler *handler, void* const *buf_ptrs,
                           const size_t *len_ptrs, size_t buf_count)
{
    assert(NULL != handler && NULL != buf_ptrs && NULL != len_ptrs && buf_count > 0);

    IORequest *io_request = IORequest::new_request(ProactHandler::READ_MASK, buf_count);
    assert(NULL != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    socket_t fd = handler->get_socket();

    DWORD bytes = 0, flags = 0;
    const int rs = ::WSARecv(fd,
                             io_request->wsabufs,
                             buf_count, // wsabuf 的数量
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
#elif NUT_PLATFORM_OS_MAC
    handler->_read_queue.push(io_request);

    if (0 == (handler->_registered_events & ProactHandler::READ_MASK))
        enable_handler(handler, ProactHandler::READ_MASK);
#elif NUT_PLATFORM_OS_LINUX
    handler->_read_queue.push(io_request);

    if (0 == (handler->_registered_events & ProactHandler::READ_MASK))
        enable_handler(handler, ProactHandler::READ_MASK);
#endif
}

void Proactor::launch_write(ProactHandler *handler, void* const *buf_ptrs,
                            const size_t *len_ptrs, size_t buf_count)
{
    assert(NULL != handler && NULL != buf_ptrs && NULL != len_ptrs && buf_count > 0);

    IORequest *io_request = IORequest::new_request(ProactHandler::WRITE_MASK, buf_count);
    assert(NULL != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    socket_t fd = handler->get_socket();
    DWORD bytes = 0;
    const int rs = ::WSASend(fd,
                             io_request->wsabufs,
                             buf_count, // wsabuf 的数量
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
#elif NUT_PLATFORM_OS_MAC
    handler->_write_queue.push(io_request);

    if (0 == (handler->_registered_events & ProactHandler::WRITE_MASK))
        enable_handler(handler, ProactHandler::WRITE_MASK);
#elif NUT_PLATFORM_OS_LINUX
    handler->_write_queue.push(io_request);

    if (0 == (handler->_registered_events & ProactHandler::WRITE_MASK))
        enable_handler(handler, ProactHandler::WRITE_MASK);
#endif
}

int Proactor::handle_events(int timeout_ms)
{
    if (_closing_or_closed)
        return -1;

    if (!nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        NUT_LOG_F(TAG, "handle_events() can only be called from loop thread");
        return -1;
    }

#if NUT_PLATFORM_OS_WINDOWS
    DWORD bytes_transfered = 0;
    void *key = NULL;
    OVERLAPPED *io_overlapped = NULL;
    BOOL rs = ::GetQueuedCompletionStatus(_iocp, &bytes_transfered, (PULONG_PTR)&key,
                                          &io_overlapped, timeout_ms);
    if (FALSE == rs)
    {
        const DWORD errcode = ::GetLastError();
        if (WAIT_TIMEOUT == errcode)
            return 0; // WAIT_TIMEOUT 表示等待超时

        NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus() with errno %d",
                  errcode);
        return -1;
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
        assert(1 == io_request->buf_count && NULL != io_request->wsabufs[0].buf);
        ::free(io_request->wsabufs[0].buf);

        handler->handle_accept_completed(io_request->accept_socket);
        break;
    }

    case ProactHandler::READ_MASK:
        handler->handle_read_completed(bytes_transfered);
        break;

    case ProactHandler::WRITE_MASK:
        handler->handle_write_completed(bytes_transfered);
        break;

    default:
        NUT_LOG_E(TAG, "unknown event type %d", io_request->event_type);
        IORequest::delete_request(io_request);
        return -1;
    }
    IORequest::delete_request(io_request);
    return 0;
#elif NUT_PLATFORM_OS_MAC
    struct timespec timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    struct kevent active_evs[MAX_ACTIVE_EVENTS];

    int n = ::kevent(_kq, NULL, 0, active_evs, MAX_ACTIVE_EVENTS, &timeout);
    for (int i = 0; i < n; ++i)
    {
        ProactHandler *handler = (ProactHandler*) active_evs[i].udata;
        assert(NULL != handler);
        socket_t fd = handler->get_socket();

        int events = active_evs[i].filter;
        if (events == EVFILT_READ)
        {
            if (handler->_request_accept > 0)
            {
                --handler->_request_accept;
                if (handler->_request_accept <= 0)
                    disable_handler(handler, ProactHandler::READ_MASK);

                while (true)
                {
                    socket_t accepted = ReactAcceptorBase::handle_accept(fd);
                    if (INVALID_SOCKET_VALUE == accepted)
                        break;
                    handler->handle_accept_completed(accepted);
                }
            }
            else
            {
                assert(!handler->_read_queue.empty());
                IORequest *io_request = (IORequest*) handler->_read_queue.front();
                assert(NULL != io_request);
                handler->_read_queue.pop();

                if (handler->_read_queue.empty())
                    disable_handler(handler, ProactHandler::READ_MASK);

                const int readed = ::readv(fd, io_request->iovs, io_request->buf_count);
                handler->handle_read_completed(readed);

                IORequest::delete_request(io_request);
            }
        }
        else if (events == EVFILT_WRITE)
        {
            assert(!handler->_write_queue.empty());
            IORequest *io_request = (IORequest*) handler->_write_queue.front();
            assert(NULL != io_request);
            handler->_write_queue.pop();

            if (handler->_write_queue.empty())
                disable_handler(handler, ProactHandler::WRITE_MASK);

            const int wrote = ::writev(fd, io_request->iovs, io_request->buf_count);
            handler->handle_write_completed(wrote);

            IORequest::delete_request(io_request);
        }
        else
        {
            NUT_LOG_E(TAG, "unknown kevent type %d", events);
            return -1;
        }
    }
    return 0;
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event events[MAX_ACTIVE_EVENTS];
    const int n = ::epoll_wait(_epoll_fd, events, MAX_ACTIVE_EVENTS, timeout_ms);
    for (int i = 0; i < n; ++i)
    {
        ProactHandler *handler = (ProactHandler*) events[i].data.ptr;
        assert(NULL != handler);
        socket_t fd = handler->get_socket();

        if (0 != (events[i].events & EPOLLIN))
        {
            if (handler->_request_accept > 0)
            {
                --handler->_request_accept;
                if (handler->_request_accept <= 0)
                    disable_handler(handler, ProactHandler::READ_MASK);

                while (true)
                {
                    socket_t accepted = ReactAcceptorBase::handle_accept(fd);
                    if (INVALID_SOCKET_VALUE == accepted)
                        break;
                    handler->handle_accept_completed(accepted);
                }
            }
            else
            {
                assert(!handler->_read_queue.empty());
                IORequest *io_request = (IORequest*) handler->_read_queue.front();
                assert(NULL != io_request);
                handler->_read_queue.pop();

                if (handler->_read_queue.empty())
                    disable_handler(handler, ProactHandler::READ_MASK);

                const int readed = ::readv(fd, io_request->iovs, io_request->buf_count);
                handler->handle_read_completed(readed);

                IORequest::delete_request(io_request);
            }
        }
        if (0 != (events[i].events & EPOLLOUT))
        {
            assert(!handler->_write_queue.empty());
            IORequest *io_request = (IORequest*) handler->_write_queue.front();
            assert(NULL != io_request);
            handler->_write_queue.pop();

            if (handler->_write_queue.empty())
                disable_handler(handler, ProactHandler::WRITE_MASK);

            const int wrote = ::writev(fd, io_request->iovs, io_request->buf_count);
            handler->handle_write_completed(wrote);

            IORequest::delete_request(io_request);
        }
    }
    return 0;
#endif

    // Run in loop tasks
    std::vector<nut::rc_ptr<nut::Runnable> > async_tasks;
    {
        nut::Guard<nut::Mutex> g(&_mutex);
        async_tasks = _async_tasks;
        _async_tasks.clear();
    }
    for (size_t i = 0, sz = async_tasks.size(); i < sz; ++i)
    {
        nut::Runnable *runnable = async_tasks.at(i);
        assert(NULL != runnable);
        runnable->run();
    }
}

void Proactor::run_in_loop_thread(nut::Runnable *runnable)
{
    assert(NULL != runnable);

    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        runnable->run();
        return;
    }

    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(runnable);
}

void Proactor::async_register_handler(ProactHandler *handler)
{
    assert(NULL != handler);

    // Synchronize
    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        register_handler(handler);
        return;
    }

    // Asynchronize
    class RegisterHandlerTask : public nut::Runnable
    {
        Proactor *_proactor;
        nut::rc_ptr<ProactHandler> _handler;

    public:
        RegisterHandlerTask(Proactor *p, ProactHandler *h)
            : _proactor(p), _handler(h)
        {}

        virtual void run() override
        {
            _proactor->register_handler(_handler);
        }
    };
    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(nut::rc_new<RegisterHandlerTask>(this, handler));
}

void Proactor::async_launch_accept(ProactHandler *handler)
{
    assert(NULL != handler);

    // Synchronize
    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        launch_accept(handler);
        return;
    }

    // Asynchronize
    class LaunchAcceptTask : public nut::Runnable
    {
        Proactor *_proactor;
        nut::rc_ptr<ProactHandler> _handler;

    public:
        LaunchAcceptTask(Proactor *p, ProactHandler *h)
            : _proactor(p), _handler(h)
        {}

        virtual void run() override
        {
            _proactor->launch_accept(_handler);
        }
    };
    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(nut::rc_new<LaunchAcceptTask>(this, handler));
}

void Proactor::async_launch_read(ProactHandler *handler, void* const *buf_ptrs,
                                 const size_t *len_ptrs, size_t buf_count)
{
    assert(NULL != handler && NULL != buf_ptrs && NULL != len_ptrs && buf_count > 0);

    // Synchronize
    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        launch_read(handler, buf_ptrs, len_ptrs, buf_count);
        return;
    }

    // Asynchronize
    class LaunchReadTask : public nut::Runnable
    {
        Proactor *_proactor;
        nut::rc_ptr<ProactHandler> _handler;
        void **_buf_ptrs;
        size_t *_len_ptrs;
        size_t _buf_count;

    public:
        LaunchReadTask(Proactor *p, ProactHandler *h, void* const *bt,
                         const size_t *lt, size_t bc)
            : _proactor(p), _handler(h), _buf_count(bc)
        {
            _buf_ptrs = (void**) ::malloc(sizeof(void*) * bc);
            ::memcpy(_buf_ptrs, bt, sizeof(void*) * bc);
            _len_ptrs = (size_t*) ::malloc(sizeof(size_t) * bc);
            ::memcpy(_len_ptrs, lt, sizeof(size_t) * bc);
        }

        virtual ~LaunchReadTask()
        {
            ::free(_buf_ptrs);
            ::free(_len_ptrs);
        }

        virtual void run() override
        {
            _proactor->launch_read(_handler, _buf_ptrs, _len_ptrs, _buf_count);
        }
    };
    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(nut::rc_new<LaunchReadTask>(this, handler, buf_ptrs, len_ptrs, buf_count));
}

void Proactor::async_launch_write(ProactHandler *handler, void* const *buf_ptrs,
                                 const size_t *len_ptrs, size_t buf_count)
{
    assert(NULL != handler && NULL != buf_ptrs && NULL != len_ptrs && buf_count > 0);

    // Synchronize
    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        launch_write(handler, buf_ptrs, len_ptrs, buf_count);
        return;
    }

    // Asynchronize
    class LaunchWriteTask : public nut::Runnable
    {
        Proactor *_proactor;
        nut::rc_ptr<ProactHandler> _handler;
        void **_buf_ptrs;
        size_t *_len_ptrs;
        size_t _buf_count;

    public:
        LaunchWriteTask(Proactor *p, ProactHandler *h, void* const *bt,
                         const size_t *lt, size_t bc)
            : _proactor(p), _handler(h), _buf_count(bc)
        {
            _buf_ptrs = (void**) ::malloc(sizeof(void*) * bc);
            ::memcpy(_buf_ptrs, bt, sizeof(void*) * bc);
            _len_ptrs = (size_t*) ::malloc(sizeof(size_t) * bc);
            ::memcpy(_len_ptrs, lt, sizeof(size_t) * bc);
        }

        virtual ~LaunchWriteTask()
        {
            ::free(_buf_ptrs);
            ::free(_len_ptrs);
        }

        virtual void run() override
        {
            _proactor->launch_write(_handler, _buf_ptrs, _len_ptrs, _buf_count);
        }
    };
    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(nut::rc_new<LaunchWriteTask>(this, handler, buf_ptrs, len_ptrs, buf_count));
}

void Proactor::async_shutdown()
{
    _closing_or_closed = true;
}

}
