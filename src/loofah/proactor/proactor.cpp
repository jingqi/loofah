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

#include "proactor.h"
#include "io_request.h"
#include "../reactor/react_acceptor.h"
#include "../inet_base/utils.h"
#include "../inet_base/error.h"


#define TAG "loofah.proactor.proactor"

namespace
{

class BufferHolder
{
    NUT_REF_COUNTABLE

public:
    BufferHolder(void* const *bp, const size_t *lp, size_t bc)
    {
        buf_ptrs = (void**) ::malloc(sizeof(void*) * bc);
        ::memcpy(buf_ptrs, bp, sizeof(void*) * bc);
        len_ptrs = (size_t*) ::malloc(sizeof(size_t) * bc);
        ::memcpy(len_ptrs, lp, sizeof(size_t) * bc);
    }

    virtual ~BufferHolder()
    {
        ::free(buf_ptrs);
        ::free(len_ptrs);
    }

private:
    BufferHolder(const BufferHolder&) = delete;
    BufferHolder& operator=(const BufferHolder&) = delete;

public:
    void **buf_ptrs;
    size_t *len_ptrs;
};

}

namespace loofah
{

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_WINDOWS
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0); // Returns nullptr if failed
    if (nullptr == _iocp)
        NUT_LOG_E(TAG, "failed to call CreateIoCompletionPort() with GetLastError() %d", ::GetLastError());
#elif NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
    if (-1 == _kq)
        LOOFAH_LOG_ERRNO(kqueue);
#elif NUT_PLATFORM_OS_LINUX
    // NOTE 自从 Linux2.6.8 版本以后，epoll_create() 参数值其实是没什么用的, 只
    //      需要大于 0
    _epoll_fd = ::epoll_create(2048);
    if (-1 == _epoll_fd)
        LOOFAH_LOG_ERRNO(epoll_create);
#endif
}

Proactor::~Proactor()
{
    shutdown();
}

void Proactor::shutdown_later()
{
    _closing_or_closed = true;
}

void Proactor::shutdown()
{
    assert(is_in_loop_thread());

    _closing_or_closed = true;

#if NUT_PLATFORM_OS_WINDOWS
    if (nullptr != _iocp)
    {
        while (true)
        {
            DWORD bytes_transfered = 0;
            void *key = nullptr;
            OVERLAPPED *io_overlapped = nullptr;
            // Get the next asynchronous operation that completes
            const BOOL rs = ::GetQueuedCompletionStatus(
                _iocp, &bytes_transfered, (PULONG_PTR)&key, &io_overlapped, 0);
            if (FALSE == rs || nullptr == io_overlapped)
                break;
            IORequest *io_request = CONTAINING_RECORD(io_overlapped, IORequest, overlapped);
            assert(nullptr != io_request);
            IORequest::delete_request(io_request);
        }
        if (FALSE == ::CloseHandle(_iocp))
            NUT_LOG_E(TAG, "failed to call CloseHandle() with GetLastError() %d", ::GetLastError());
    }
    _iocp = nullptr;
#elif NUT_PLATFORM_OS_MAC
    if (-1 != _kq)
    {
        if (0 != ::close(_kq))
            LOOFAH_LOG_ERRNO(close);
    }
    _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    if (-1 != _epoll_fd)
    {
        if (0 != ::close(_epoll_fd))
            LOOFAH_LOG_ERRNO(close);
    }
    _epoll_fd = -1;
#endif
}

void Proactor::register_handler_later(ProactHandler *handler)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
    {
        // Synchronize
        register_handler(handler);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        add_later_task([=] { register_handler(ref_handler); });
    }
}

void Proactor::register_handler(ProactHandler *handler)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    assert(nullptr != _iocp);
    const HANDLE rs = ::CreateIoCompletionPort((HANDLE) fd, _iocp, (ULONG_PTR) handler, 0);
    if (nullptr == rs)
    {
        NUT_LOG_E(TAG, "failed to call CreateIoCompletionPort() with GetLastError() %d", ::GetLastError());
        return;
    }
    assert(rs == _iocp);
#elif NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    EV_SET(ev, fd, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    epv.events = 0;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &epv))
    {
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
        return;
    }
#endif
}

void Proactor::unregister_handler_later(ProactHandler *handler)
{
    assert(nullptr != handler);

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 对于 Windows 下的 IOCP，是无法 unregister_handler() 的
#else
    if (is_in_loop_thread())
    {
        // Synchronize
        unregister_handler(handler);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        add_later_task([=] { unregister_handler(ref_handler); });
    }
#endif
}

void Proactor::unregister_handler(ProactHandler *handler)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 对于 Windows 下的 IOCP，是无法 unregister_handler() 的
    NUT_LOG_W(TAG, "Proactor::unregister_handler() not implemented in Windows");
#elif NUT_PLATFORM_OS_MAC
    const socket_t fd = handler->get_socket();
    struct kevent ev[2];
    EV_SET(ev, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    EV_SET(ev + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, 2, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        return;
    }
#elif NUT_PLATFORM_OS_LINUX
    const socket_t fd = handler->get_socket();
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.ptr = (void*) handler;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &epv))
    {
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
        return;
    }
#endif
}

void Proactor::launch_accept_later(ProactHandler *handler)
{
    assert(nullptr != handler);

    if (is_in_loop_thread())
    {
        // Synchronize
        launch_accept(handler);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        add_later_task([=] { launch_accept(ref_handler); });
    }
}

void Proactor::launch_accept(ProactHandler *handler)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t listener_socket = handler->get_socket();

#if NUT_PLATFORM_OS_WINDOWS
    // Create socket
    socket_t accept_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == accept_socket)
    {
        LOOFAH_LOG_ERRNO(WSASocket);
        return;
    }

    // Call ::AcceptEx()
    IORequest *io_request = IORequest::new_request(ProactHandler::ACCEPT_MASK,
                                                   1, accept_socket);
    assert(nullptr != io_request);
    const size_t buf_len = 2 * (sizeof(struct sockaddr_in) + 16);
    void *buf = ::malloc(2 * (sizeof(struct sockaddr_in) + 16));
    assert(nullptr != buf);
    io_request->set_buf(0, buf, buf_len);

    DWORD bytes = 0;
    assert(nullptr != func_AcceptEx);
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
            LOOFAH_LOG_FD_ERRNO(AcceptEx, listener_socket);
            if (0 != ::closesocket(accept_socket))
                LOOFAH_LOG_FD_ERRNO(closesocket, accept_socket);
            handler->handle_exception(from_errno(errcode));
            return;
        }
    }
#elif NUT_PLATFORM_OS_MAC
    ++handler->_request_accept;
    if (0 == (handler->_registered_events & ProactHandler::READ_MASK))
    {
        struct kevent ev;
        EV_SET(&ev, listener_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
        if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
        {
            LOOFAH_LOG_FD_ERRNO(kevent, listener_socket);
            handler->handle_exception(from_errno(errno));
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
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, listener_socket);
            handler->handle_exception(from_errno(errno));
            return;
        }
        handler->_registered_events |= EPOLLIN;
    }
#endif
}

void Proactor::launch_read_later(ProactHandler *handler, void* const *buf_ptrs,
                                 const size_t *len_ptrs, size_t buf_count)
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);

    if (is_in_loop_thread())
    {
        // Synchronize
        launch_read(handler, buf_ptrs, len_ptrs, buf_count);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        nut::rc_ptr<BufferHolder> buf_holder = nut::rc_new<BufferHolder>(buf_ptrs, len_ptrs, buf_count);
        add_later_task([=] { launch_read(ref_handler, buf_holder->buf_ptrs, buf_holder->len_ptrs, buf_count); });
    }
}

void Proactor::launch_read(ProactHandler *handler, void* const *buf_ptrs,
                           const size_t *len_ptrs, size_t buf_count)
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);
    assert(is_in_loop_thread());

    IORequest *io_request = IORequest::new_request(ProactHandler::READ_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    const socket_t fd = handler->get_socket();

    DWORD bytes = 0, flags = 0;
    const int rs = ::WSARecv(fd,
                             io_request->wsabufs,
                             buf_count, // wsabuf 的数量
                             &bytes, // 如果接收操作立即完成，这里会返回函数调用所接收到的字节数
                             &flags, // FIXME 貌似这里设置为 nullptr 会导致错误
                             &io_request->overlapped,
                             nullptr);
    if (SOCKET_ERROR == rs)
    {
        const int errcode = ::WSAGetLastError();
        if (ERROR_IO_PENDING != errcode)
        {
            LOOFAH_LOG_FD_ERRNO(WSARecv, fd);
            handler->handle_exception(from_errno(errcode));
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

void Proactor::launch_write_later(ProactHandler *handler, void* const *buf_ptrs,
                                  const size_t *len_ptrs, size_t buf_count)
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);

    if (is_in_loop_thread())
    {
        // Synchronize
        launch_write(handler, buf_ptrs, len_ptrs, buf_count);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        nut::rc_ptr<BufferHolder> buf_holder = nut::rc_new<BufferHolder>(buf_ptrs, len_ptrs, buf_count);
        add_later_task([=] { launch_write(ref_handler, buf_holder->buf_ptrs, buf_holder->len_ptrs, buf_count); });
    }
}

void Proactor::launch_write(ProactHandler *handler, void* const *buf_ptrs,
                            const size_t *len_ptrs, size_t buf_count)
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);
    assert(is_in_loop_thread());

    IORequest *io_request = IORequest::new_request(ProactHandler::WRITE_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

#if NUT_PLATFORM_OS_WINDOWS
    const socket_t fd = handler->get_socket();
    DWORD bytes = 0;
    const int rs = ::WSASend(fd,
                             io_request->wsabufs,
                             buf_count, // wsabuf 的数量
                             &bytes, // 如果发送操作立即完成，这里会返回函数调用所发送的字节数
                             0,
                             &io_request->overlapped,
                             nullptr);
    if (SOCKET_ERROR == rs)
    {
        const int errcode = ::WSAGetLastError();
        if (ERROR_IO_PENDING != errcode)
        {
            LOOFAH_LOG_FD_ERRNO(WSASend, fd);
            handler->handle_exception(from_errno(errcode));
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

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
void Proactor::enable_handler(ProactHandler *handler, int mask)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

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
    if (0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_exception(from_errno(errno));
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
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
        handler->handle_exception(from_errno(errno));
        return;
    }
#endif

    handler->_registered_events |= mask;
}

void Proactor::disable_handler(ProactHandler *handler, int mask)
{
    assert(nullptr != handler);
    assert(is_in_loop_thread());

    const socket_t fd = handler->get_socket();

#if NUT_PLATFORM_OS_MAC
    struct kevent ev[2];
    int n = 0;
    if (0 != (mask & ProactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (mask & ProactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_exception(from_errno(errno));
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
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
        handler->handle_exception(from_errno(errno));
        return;
    }
#endif

    handler->_registered_events &= ~mask;
}
#endif

int Proactor::handle_events(int timeout_ms)
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
        DWORD bytes_transfered = 0;
        void *key = nullptr;
        OVERLAPPED *io_overlapped = nullptr;
        assert(nullptr != _iocp);
        BOOL rs = ::GetQueuedCompletionStatus(_iocp, &bytes_transfered, (PULONG_PTR) &key,
                                              &io_overlapped, timeout_ms);
        // NOTE 返回值为 False, 但是返回的 io_overlapped 不为 nullptr, 则仅仅说明链接被中断了
        if (FALSE == rs && nullptr == io_overlapped)
        {
            const DWORD errcode = ::GetLastError();
            // NOTE WAIT_TIMEOUT 表示等待超时;
            //      timeout_ms 传入 0, 而无事件可处理, 触发 ERROR_SUCCESS 错误;
            if (WAIT_TIMEOUT != errcode && ERROR_SUCCESS != errcode)
            {
                NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus() with GetLastError() %d",
                          errcode);
                return -1;
            }
        }
        else
        {
            assert(FALSE != rs || 0 == bytes_transfered);

            ProactHandler *handler = (ProactHandler*) key;
            assert(nullptr != handler);

            assert(nullptr != io_overlapped);
            IORequest *io_request = CONTAINING_RECORD(io_overlapped, IORequest, overlapped);
            assert(nullptr != io_request);

            switch (io_request->event_type)
            {
            case ProactHandler::ACCEPT_MASK:
            {
                /* Get peer address
                 *
                struct sockaddr_in *remote_addr = nullptr, *local_addr = nullptr;
                int remote_len = sizeof(struct sockaddr_in), local_len = sizeof(struct sockaddr_in);
                assert(nullptr != func_GetAcceptExSockaddrs);
                func_GetAcceptExSockaddrs(io_request->wsabuf.buf,
                                          io_request->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16),
                                          sizeof(struct sockaddr_in) + 16,
                                          sizeof(struct sockaddr_in) + 16,
                                          (LPSOCKADDR*)&local_addr,
                                          &local_len,
                                          (LPSOCKADDR*)&remote_addr,
                                          &remote_len);
                 */
                assert(1 == io_request->buf_count && nullptr != io_request->wsabufs[0].buf);
                ::free(io_request->wsabufs[0].buf);

                handler->handle_accept_completed(io_request->accept_socket);
                break;
            }

            case ProactHandler::READ_MASK:
                handler->handle_read_completed((size_t) bytes_transfered);
                break;

            case ProactHandler::WRITE_MASK:
                handler->handle_write_completed((size_t) bytes_transfered);
                break;

            default:
                NUT_LOG_E(TAG, "unknown event type %d", io_request->event_type);
                IORequest::delete_request(io_request);
                return -1;
            }
            IORequest::delete_request(io_request);
        }
#elif NUT_PLATFORM_OS_MAC
        struct timespec timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
        struct kevent active_evs[LOOFAH_MAX_ACTIVE_EVENTS];

        int n = ::kevent(_kq, nullptr, 0, active_evs, LOOFAH_MAX_ACTIVE_EVENTS, &timeout);
        for (int i = 0; i < n; ++i)
        {
            ProactHandler *handler = (ProactHandler*) active_evs[i].udata;
            assert(nullptr != handler);
            const socket_t fd = handler->get_socket();

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
                        if (LOOFAH_INVALID_SOCKET_FD == accepted)
                            break;
                        handler->handle_accept_completed(accepted);
                    }
                }
                else
                {
                    assert(!handler->_read_queue.empty());
                    IORequest *io_request = handler->_read_queue.front();
                    assert(nullptr != io_request);
                    handler->_read_queue.pop();

                    if (handler->_read_queue.empty())
                        disable_handler(handler, ProactHandler::READ_MASK);

                    const ssize_t readed = ::readv(fd, io_request->iovs, io_request->buf_count);
                    if (readed >= 0)
                        handler->handle_read_completed((size_t) readed);
                    else
                        handler->handle_exception(from_errno(errno));

                    IORequest::delete_request(io_request);
                }
            }
            else if (events == EVFILT_WRITE)
            {
                assert(!handler->_write_queue.empty());
                IORequest *io_request = handler->_write_queue.front();
                assert(nullptr != io_request);
                handler->_write_queue.pop();

                if (handler->_write_queue.empty())
                    disable_handler(handler, ProactHandler::WRITE_MASK);

                const ssize_t wrote = ::writev(fd, io_request->iovs, io_request->buf_count);
                if (wrote >= 0)
                    handler->handle_write_completed((size_t) wrote);
                else
                    handler->handle_exception(from_errno(errno));

                IORequest::delete_request(io_request);
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
            ProactHandler *handler = (ProactHandler*) events[i].data.ptr;
            assert(nullptr != handler);
            const socket_t fd = handler->get_socket();

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
                        if (LOOFAH_INVALID_SOCKET_FD == accepted)
                            break;
                        handler->handle_accept_completed(accepted);
                    }
                }
                else
                {
                    assert(!handler->_read_queue.empty());
                    IORequest *io_request = handler->_read_queue.front();
                    assert(nullptr != io_request);
                    handler->_read_queue.pop();

                    if (handler->_read_queue.empty())
                        disable_handler(handler, ProactHandler::READ_MASK);

                    const ssize_t readed = ::readv(fd, io_request->iovs, io_request->buf_count);
                    if (readed >= 0)
                        handler->handle_read_completed((size_t) readed);
                    else
                        handler->handle_exception(from_errno(errno));

                    IORequest::delete_request(io_request);
                }
            }
            if (0 != (events[i].events & EPOLLOUT))
            {
                assert(!handler->_write_queue.empty());
                IORequest *io_request = handler->_write_queue.front();
                assert(nullptr != io_request);
                handler->_write_queue.pop();

                if (handler->_write_queue.empty())
                    disable_handler(handler, ProactHandler::WRITE_MASK);

                const ssize_t wrote = ::writev(fd, io_request->iovs, io_request->buf_count);
                if (wrote >= 0)
                    handler->handle_write_completed((size_t) wrote);
                else
                    handler->handle_exception(from_errno(errno));

                IORequest::delete_request(io_request);
            }
        }
#endif

    }

    // Run asynchronized tasks
    run_later_tasks();

    return 0;
}

}
