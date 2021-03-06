﻿/**
 * 1. 关于 Windows 下的完成端口，参见 http://blog.csdn.net/piggyxp/article/details/6922277
 * 2. 关于 Linux 下 libaio + epoll，参见 http://blog.chinaunix.net/uid-16979052-id-3840266.html
 */

#include "../loofah_config.h"

#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#elif NUT_PLATFORM_OS_MACOS
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h>
#   include <sys/eventfd.h>
#   include <unistd.h> // for ::close()
#   include <errno.h>
#endif

#include <nut/logging/logger.h>

#include "../reactor/react_acceptor.h"
#include "../inet_base/utils.h"
#include "../inet_base/error.h"
#include "../inet_base/sock_operation.h"
#include "proactor.h"
#include "io_request.h"


// Magic number
#define KQUEUE_WAKEUP_IDENT 0

#define TAG "loofah.proactor"

namespace loofah
{

namespace
{

#if NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
ProactHandler::mask_type real_mask(ProactHandler::mask_type mask) noexcept
{
    ProactHandler::mask_type ret = 0;
    if (0 != (mask & ProactHandler::ACCEPT_READ_MASK))
        ret |= ProactHandler::READ_MASK;
    if (0 != (mask & ProactHandler::CONNECT_WRITE_MASK))
        ret |= ProactHandler::WRITE_MASK;
    return ret;
}
#endif

}

Proactor::Proactor() noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    // Create IOCP
    const DWORD concurrent_threads = 1; // NOTE 目前我们只允许一个线程去执行 IOCP
    _iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, concurrent_threads);
    if (nullptr == _iocp)
        NUT_LOG_E(TAG, "failed to call CreateIoCompletionPort() with GetLastError() %d", ::GetLastError());
#elif NUT_PLATFORM_OS_MACOS
    // Create kqueue
    _kq = ::kqueue();
    if (_kq < 0)
    {
        LOOFAH_LOG_ERRNO(kqueue);
        return;
    }

    // Register user event filter
    struct kevent ev;
    EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, NOTE_FFNOP, 0, nullptr);
    if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
        LOOFAH_LOG_FD_ERRNO(kevent, _kq);
#elif NUT_PLATFORM_OS_LINUX
    // Create epoll fd
    // NOTE 自从 Linux2.6.8 版本以后，epoll_create() 参数值其实是没什么用的, 只
    //      需要大于 0
    _epoll_fd = ::epoll_create(2048);
    if (_epoll_fd < 0)
    {
        LOOFAH_LOG_ERRNO(epoll_create);
        return;
    }

    // Create eventfd
    _event_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (_event_fd < 0)
    {
        LOOFAH_LOG_ERRNO(eventfd);
        return;
    }

    // Register eventfd to epoll fd
    struct epoll_event epv;
    ::memset(&epv, 0, sizeof(epv));
    epv.data.fd = _event_fd;
    epv.events = EPOLLIN | EPOLLERR;
    if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _event_fd, &epv))
        LOOFAH_LOG_FD_ERRNO(epoll_ctl, _event_fd);
#endif
}

Proactor::~Proactor() noexcept
{
    shutdown();
}

void Proactor::shutdown_later() noexcept
{
    _closing_or_closed.store(true, std::memory_order_relaxed);

    if (!is_in_io_thread())
        wakeup_poll_wait();
}

void Proactor::shutdown() noexcept
{
    assert(is_in_io_thread());

    _closing_or_closed.store(true, std::memory_order_relaxed);

#if NUT_PLATFORM_OS_WINDOWS
    // Close IOCP
    if (nullptr != _iocp)
    {
        // NOTE 如果有多个线程等待在这个 iocp 上, 需要用 ::PostQueuedCompletionStatus()
        //      唤醒所有等待线程, 不过这里我们只有一个线程且处于非等待状态, 不需
        //      要这样做

        if (FALSE == ::CloseHandle(_iocp))
            NUT_LOG_E(TAG, "failed to call CloseHandle() with GetLastError() %d", ::GetLastError());
    }
    _iocp = nullptr;
#elif NUT_PLATFORM_OS_MACOS
    if (_kq >= 0)
    {
        // Unregister user event filter
        struct kevent ev;
        EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, EV_DELETE, 0, 0, nullptr);
        if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
            LOOFAH_LOG_FD_ERRNO(kevent, _kq);

        // Close kqueue
        if (0 != ::close(_kq))
            LOOFAH_LOG_ERRNO(close);
    }
    _kq = -1;
#elif NUT_PLATFORM_OS_LINUX
    // Unregister eventfd from epoll fd
    if (_event_fd >= 0 && _epoll_fd >= 0)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.fd = _event_fd;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _event_fd, &epv))
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, _event_fd);
    }

    // Close eventfd
    if (_event_fd >= 0)
    {
        if (0 != ::close(_event_fd))
            LOOFAH_LOG_ERRNO(close);
    }
    _event_fd = -1;

    // Close epoll fd
    if (_epoll_fd >= 0)
    {
        if (0 != ::close(_epoll_fd))
            LOOFAH_LOG_ERRNO(close);
    }
    _epoll_fd = -1;
#endif
}

void Proactor::register_handler_later(ProactHandler *handler) noexcept
{
    assert(nullptr != handler);

    if (is_in_io_thread())
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

void Proactor::register_handler(ProactHandler *handler) noexcept
{
    assert(nullptr != handler && nullptr == handler->_registered_proactor);
    assert(is_in_io_thread());

#if NUT_PLATFORM_OS_WINDOWS
    const socket_t fd = handler->get_socket();
    if (_associated_sockets.find(fd) == _associated_sockets.end())
    {
        assert(nullptr != _iocp);
        const HANDLE rs = ::CreateIoCompletionPort((HANDLE) fd, _iocp, (ULONG_PTR) nullptr, 0);
        if (nullptr == rs)
        {
            NUT_LOG_E(TAG, "failed to call CreateIoCompletionPort() with GetLastError() %d", ::GetLastError());
            handler->handle_io_error(LOOFAH_ERR_UNKNOWN);
            return;
        }
        assert(rs == _iocp);
        _associated_sockets.insert(fd);
    }
#elif NUT_PLATFORM_OS_MACOS
    assert(0 == handler->_registered_events && 0 == handler->_enabled_events);
#elif NUT_PLATFORM_OS_LINUX
    assert(!handler->_registered && 0 == handler->_enabled_events);
#endif

    handler->_registered_proactor = this;
}

void Proactor::unregister_handler_later(ProactHandler *handler) noexcept
{
    assert(nullptr != handler);

    if (is_in_io_thread())
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
}

void Proactor::unregister_handler(ProactHandler *handler) noexcept
{
    assert(nullptr != handler && handler->_registered_proactor == this);
    assert(is_in_io_thread());


#if NUT_PLATFORM_OS_WINDOWS
    // FIXME 对于 Windows 下的 IOCP，是无法取消 socket 与 iocp 的关联的
    const socket_t fd = handler->get_socket();
#   if WINVER >= _WIN32_WINNT_WINBLUE
    ::CancelIoEx((HANDLE) fd, nullptr); // 取消等待执行的异步操作
#   else
    ::CancelIo((HANDLE) fd); // 取消当前线程注册的尚未完成的异步操作，这里都是在一个线程中发起的异步操作
#   endif
    handler->_registered_proactor = nullptr;
    handler->delete_requests();
#elif NUT_PLATFORM_OS_MACOS
    const socket_t fd = handler->get_socket();
    struct kevent ev[2];
    int n = 0;
    if (0 != (handler->_registered_events & ProactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DELETE, 0, 0, (void*) handler);
    if (0 != (handler->_registered_events & ProactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*) handler);
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_registered_events = 0;
    handler->_enabled_events = 0;
    handler->_registered_proactor = nullptr;
    handler->delete_requests();
#elif NUT_PLATFORM_OS_LINUX
    if (handler->_registered)
    {
        const socket_t fd = handler->get_socket();
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_registered = false;
    handler->_enabled_events = 0;
    handler->_registered_proactor = nullptr;
    handler->delete_requests();
#endif
}

void Proactor::launch_accept_later(ProactHandler *handler) noexcept
{
    assert(nullptr != handler);

    if (is_in_io_thread())
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

void Proactor::launch_accept(ProactHandler *handler) noexcept
{
    assert(nullptr != handler && handler->_registered_proactor == this);
    assert(is_in_io_thread());

#if NUT_PLATFORM_OS_WINDOWS
    // Create socket
    const socket_t accept_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, nullptr,
                                               0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == accept_socket)
    {
        LOOFAH_LOG_ERRNO(WSASocket);
        return;
    }

    // Call ::AcceptEx()
    // NOTE 'buf' 中存放本地地址、对端地址、首个接收数据(可以是0长度)。且存放地
    //      址的空间必须比所用传输协议的最大地址大16个字节
    IORequest *io_request = IORequest::new_request(handler, ProactHandler::ACCEPT_MASK, 1, accept_socket);
    assert(nullptr != io_request);
    const size_t buf_len = 2 * (sizeof(struct sockaddr_in) + 16);
    void *buf = ::malloc(2 * (sizeof(struct sockaddr_in) + 16));
    assert(nullptr != buf);
    io_request->set_buf(0, buf, buf_len);

    const socket_t listening_socket = handler->get_socket();
    DWORD bytes = 0;
    assert(nullptr != func_AcceptEx);
    const BOOL rs = func_AcceptEx(listening_socket,
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
            LOOFAH_LOG_FD_ERRNO(AcceptEx, listening_socket);
            if (0 != ::closesocket(accept_socket))
                LOOFAH_LOG_FD_ERRNO(closesocket, accept_socket);
            IORequest::delete_request(io_request);
            handler->handle_io_error(from_errno(errcode));
            return;
        }
    }
    handler->_read_queue.push(io_request);
#elif NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
    ++handler->_request_accept;
    enable_handler(handler, ProactHandler::ACCEPT_MASK);
#endif
}

#if NUT_PLATFORM_OS_WINDOWS
void Proactor::launch_connect_later(ProactHandler *handler, const InetAddr& address) noexcept
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        launch_connect(handler, address);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        add_later_task([=] { launch_connect(ref_handler, address); });
    }
}
#else
void Proactor::launch_connect_later(ProactHandler *handler) noexcept
{
    assert(nullptr != handler);

    if (is_in_io_thread())
    {
        // Synchronize
        launch_connect(handler);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);
        add_later_task([=] { launch_connect(ref_handler); });
    }
}
#endif


#if NUT_PLATFORM_OS_WINDOWS
void Proactor::launch_connect(ProactHandler *handler, const InetAddr& address) noexcept
{
    assert(nullptr != handler && handler->_registered_proactor == this);
    assert(is_in_io_thread());

    IORequest *io_request = IORequest::new_request(handler, ProactHandler::CONNECT_MASK);
    assert(nullptr != io_request);

    const socket_t fd = handler->get_socket();
    DWORD bytes = 0;
    const BOOL rs = func_ConnectEx(fd,
                                   address.cast_to_sockaddr(),
                                   address.get_sockaddr_size(),
                                   nullptr,
                                   0,
                                   &bytes,
                                   &io_request->overlapped);
    if (FALSE == rs)
    {
        const int errcode = ::WSAGetLastError();
        // NOTE ERROR_IO_PENDING 说明异步调用没有可立即处理的数据，属于正常情况
        if (ERROR_IO_PENDING != errcode)
        {
            LOOFAH_LOG_FD_ERRNO(ConnectEx, fd);
            IORequest::delete_request(io_request);
            handler->handle_io_error(from_errno(errcode));
            return;
        }
    }
    handler->_write_queue.push(io_request);
}
#else
void Proactor::launch_connect(ProactHandler *handler) noexcept
{
    assert(nullptr != handler && handler->_registered_proactor == this);
    assert(is_in_io_thread());
    assert(0 == (handler->_enabled_events & ProactHandler::CONNECT_MASK));
    enable_handler(handler, ProactHandler::CONNECT_MASK);
}
#endif


void Proactor::launch_read_later(ProactHandler *handler, void* const *buf_ptrs,
                                 const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);

    if (is_in_io_thread())
    {
        // Synchronize
        launch_read(handler, buf_ptrs, len_ptrs, buf_count);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);

        void **dup_buf_ptrs = (void**) ::malloc(sizeof(void*) * buf_count + sizeof(size_t) * buf_count);
        ::memcpy(dup_buf_ptrs, buf_ptrs, sizeof(void*) * buf_count);
        size_t *dup_len_ptrs = (size_t*) (dup_buf_ptrs + buf_count);
        ::memcpy(dup_len_ptrs, len_ptrs, sizeof(size_t) * buf_count);

        add_later_task([=] {
            launch_read(ref_handler, dup_buf_ptrs, dup_len_ptrs, buf_count);
            ::free(dup_buf_ptrs);
        });
    }
}

void Proactor::launch_read(ProactHandler *handler, void* const *buf_ptrs,
                           const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);
    assert(handler->_registered_proactor == this);
    assert(is_in_io_thread());

#if NUT_PLATFORM_OS_WINDOWS
    IORequest *io_request = IORequest::new_request(handler, ProactHandler::READ_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

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
            IORequest::delete_request(io_request);
            handler->handle_io_error(from_errno(errcode));
            return;
        }
    }
    handler->_read_queue.push(io_request);
#elif NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
    IORequest *io_request = IORequest::new_request(ProactHandler::READ_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

    handler->_read_queue.push(io_request);
    enable_handler(handler, ProactHandler::READ_MASK);
#endif
}

void Proactor::launch_write_later(ProactHandler *handler, void* const *buf_ptrs,
                                  const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);

    if (is_in_io_thread())
    {
        // Synchronize
        launch_write(handler, buf_ptrs, len_ptrs, buf_count);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactHandler> ref_handler(handler);

        void **dup_buf_ptrs = (void**) ::malloc(sizeof(void*) * buf_count + sizeof(size_t) * buf_count);
        ::memcpy(dup_buf_ptrs, buf_ptrs, sizeof(void*) * buf_count);
        size_t *dup_len_ptrs = (size_t*) (dup_buf_ptrs + buf_count);
        ::memcpy(dup_len_ptrs, len_ptrs, sizeof(size_t) * buf_count);

        add_later_task([=] {
            launch_write(ref_handler, dup_buf_ptrs, dup_len_ptrs, buf_count);
            ::free(dup_buf_ptrs);
        });
    }
}

void Proactor::launch_write(ProactHandler *handler, void* const *buf_ptrs,
                            const size_t *len_ptrs, size_t buf_count) noexcept
{
    assert(nullptr != handler && nullptr != buf_ptrs && nullptr != len_ptrs && buf_count > 0);
    assert(handler->_registered_proactor == this);
    assert(is_in_io_thread());

#if NUT_PLATFORM_OS_WINDOWS
    IORequest *io_request = IORequest::new_request(handler, ProactHandler::WRITE_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

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
            IORequest::delete_request(io_request);
            handler->handle_io_error(from_errno(errcode));
            return;
        }
    }
    handler->_write_queue.push(io_request);
#elif NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
    IORequest *io_request = IORequest::new_request(ProactHandler::WRITE_MASK, buf_count);
    assert(nullptr != io_request);
    io_request->set_bufs(buf_ptrs, len_ptrs);

    handler->_write_queue.push(io_request);
    enable_handler(handler, ProactHandler::WRITE_MASK);
#endif
}

#if NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
void Proactor::enable_handler(ProactHandler *handler, ProactHandler::mask_type mask) noexcept
{
    assert(nullptr != handler && 0 == (mask & ~ProactHandler::ALL_MASK));
    assert(handler->_registered_proactor == this);
    assert(is_in_io_thread());

    if (0 == mask)
        return;
    const socket_t fd = handler->get_socket();
    const ProactHandler::mask_type need_enable = ~real_mask(handler->_enabled_events) & real_mask(mask);

#if NUT_PLATFORM_OS_MACOS
    struct kevent ev[2];
    int n = 0;
    if (0 != (need_enable & ProactHandler::READ_MASK))
    {
        if (0 != (handler->_registered_events & ProactHandler::READ_MASK))
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (0 != (need_enable & ProactHandler::WRITE_MASK))
    {
        if (0 != (handler->_registered_events & ProactHandler::WRITE_MASK))
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ENABLE, 0, 0, (void*) handler);
        else
            EV_SET(ev + n++, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, (void*) handler);
    }
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_registered_events |= need_enable;
    handler->_enabled_events |= mask;
#elif NUT_PLATFORM_OS_LINUX
    if (0 != need_enable)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        const ProactHandler::mask_type final_enabled = handler->_enabled_events | mask;
        if (0 != (final_enabled & ProactHandler::ACCEPT_READ_MASK))
            epv.events |= EPOLLIN | EPOLLERR;
        if (0 != (final_enabled & ProactHandler::CONNECT_WRITE_MASK))
            epv.events |= EPOLLOUT | EPOLLERR;
        if (0 != ::epoll_ctl(_epoll_fd, (handler->_registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD), fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_registered = true;
    handler->_enabled_events |= mask;
#endif
}

void Proactor::disable_handler(ProactHandler *handler, ProactHandler::mask_type mask) noexcept
{
    assert(nullptr != handler && 0 == (mask & ~ProactHandler::ALL_MASK));
    assert(handler->_registered_proactor == this);
    assert(is_in_io_thread());

    if (0 == mask)
        return;
    const socket_t fd = handler->get_socket();
    const ProactHandler::mask_type final_enabled = real_mask(handler->_enabled_events & ~mask),
        need_disable = real_mask(handler->_enabled_events) & ~final_enabled;

#if NUT_PLATFORM_OS_MACOS
    struct kevent ev[2];
    int n = 0;
    if (0 != (need_disable & ProactHandler::READ_MASK))
        EV_SET(ev + n++, fd, EVFILT_READ, EV_DISABLE, 0, 0, (void*) handler);
    if (0 != (need_disable & ProactHandler::WRITE_MASK))
        EV_SET(ev + n++, fd, EVFILT_WRITE, EV_DISABLE, 0, 0, (void*) handler);
    if (n > 0 && 0 != ::kevent(_kq, ev, n, nullptr, 0, nullptr))
    {
        LOOFAH_LOG_FD_ERRNO(kevent, fd);
        handler->handle_io_error(from_errno(errno));
        return;
    }
    handler->_enabled_events &= ~mask;
#elif NUT_PLATFORM_OS_LINUX
    if (0 != need_disable)
    {
        struct epoll_event epv;
        ::memset(&epv, 0, sizeof(epv));
        epv.data.ptr = (void*) handler;
        const ProactHandler::mask_type final_enabled = handler->_enabled_events & ~mask;
        if (0 != (final_enabled & ProactHandler::READ_MASK))
            epv.events |= EPOLLIN | EPOLLERR;
        if (0 != (final_enabled & ProactHandler::WRITE_MASK))
            epv.events |= EPOLLOUT | EPOLLERR;
        if (0 != ::epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &epv))
        {
            LOOFAH_LOG_FD_ERRNO(epoll_ctl, fd);
            handler->handle_io_error(from_errno(errno));
            return;
        }
    }
    handler->_enabled_events &= ~mask;
#endif
}
#endif

int Proactor::poll(int timeout_ms) noexcept
{
    if (_closing_or_closed.load(std::memory_order_relaxed))
        return -1;

    if (!is_in_io_thread())
    {
        NUT_LOG_F(TAG, "poll() can only be called from inside IO thread");
        return -1;
    }

#if NUT_PLATFORM_OS_WINDOWS
    const DWORD timeout = (timeout_ms < 0 ? INFINITE : timeout_ms);
    DWORD bytes_transfered = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *io_overlapped = nullptr;
    assert(nullptr != _iocp);
    /**
     * GetQueuedCompletionStatus() 返回值处理
     * - rs == FALSE
     *   - io_overlapped == NULL
     *     - errcode == WAIT_TIMEOUT 超时                                        (case 1)
     *     - 错误                                                                (case 2)
     *   - io_overlapped != NULL
     *     - errcode == ERROR_SUCESS && bytes_transfered == 0 连接关闭           (case 3)
     *     - 连接错误                                                            (case 4)
     * - rs != FALSE
     *   - io_overlapped == NULL 收到通知 (由 PostQueuedCompletionStatus() 发送) (case 5)
     *   - io_overlapped != NULL, bytes_transfered > 0 正常返回                  (case 6)
     */
    _poll_stage = PollStage::PollingWait;
    const BOOL rs = ::GetQueuedCompletionStatus(_iocp, &bytes_transfered, &key,
                                                &io_overlapped, timeout);
    _poll_stage = PollStage::HandlingEvents;
    const DWORD errcode = (FALSE == rs ? ::GetLastError() : ERROR_SUCCESS);
    if (nullptr == io_overlapped) // case 1, 2, 5: 超时 / 错误 / 收到通知
    {
        if (FALSE == rs && WAIT_TIMEOUT != errcode) // case 2: 错误
        {
            NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus() with ::GetLastError() %d",
                      errcode);
            return -1;
        }
        // 忽略 case 1, 5
    }
    else if (FALSE == rs && ERROR_SUCCESS != errcode) // case 4: 连接错误
    {
        IORequest *io_request = CONTAINING_RECORD(io_overlapped, IORequest, overlapped);
        assert(nullptr != io_request);

        ProactHandler *handler = io_request->handler;
        assert(nullptr != handler);
        if (0 != (io_request->event_type & ProactHandler::ACCEPT_READ_MASK))
        {
            assert(io_request == handler->_read_queue.front());
            handler->_read_queue.pop();
        }
        else
        {
            assert(io_request == handler->_write_queue.front());
            handler->_write_queue.pop();
        }
        IORequest::delete_request(io_request);

        // FIXME 因为 ::GetQueuedCompletionStatus() 不返回底层网络驱动的错误
        //       码，导致低层网络驱动错误码被丢失
        //       Also see https://stackoverflow.com/questions/28925003/calling-wsagetlasterror-from-an-iocp-thread-return-incorrect-result
        NUT_LOG_W(TAG, "failed to call ::GetQueuedCompletionStatus() with ::GetLastError() %d",
                  errcode);
        handler->handle_io_error(LOOFAH_ERR_UNKNOWN); // 连接错误
    }
    else // case 3, 6: 连接关闭 / 正常返回
    {
        assert(FALSE != rs || (ERROR_SUCCESS == errcode && 0 == bytes_transfered));

        IORequest *io_request = CONTAINING_RECORD(io_overlapped, IORequest, overlapped);
        assert(nullptr != io_request);

        ProactHandler *handler = io_request->handler;
        assert(nullptr != handler);
        if (0 != (io_request->event_type & ProactHandler::ACCEPT_READ_MASK))
        {
            assert(io_request == handler->_read_queue.front());
            handler->_read_queue.pop();
        }
        else
        {
            assert(io_request == handler->_write_queue.front());
            handler->_write_queue.pop();
        }

        switch (io_request->event_type)
        {
        case ProactHandler::ACCEPT_MASK:
        {
            /*
             * Get peer address if needed:
             *
             * struct sockaddr_in *remote_addr = nullptr, *local_addr = nullptr;
             * int remote_len = sizeof(struct sockaddr_in), local_len = sizeof(struct sockaddr_in);
             * assert(nullptr != func_GetAcceptExSockaddrs);
             * func_GetAcceptExSockaddrs(io_request->wsabuf.buf,
             *                           io_request->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16),
             *                           sizeof(struct sockaddr_in) + 16,
             *                           sizeof(struct sockaddr_in) + 16,
             *                           (LPSOCKADDR*)&local_addr,
             *                           &local_len,
             *                           (LPSOCKADDR*)&remote_addr,
             *                           &remote_len);
             */
            assert(1 == io_request->buf_count && nullptr != io_request->wsabufs[0].buf);
            ::free(io_request->wsabufs[0].buf);

            // 把 listening 套结字一些属性(包括 socket 内部接受/发送缓存大小
            // 等等)拷贝到新建立的套结字，可以使后续的 shutdown() 调用成功
            const socket_t listening_socket = handler->get_socket(),
                accept_socket = io_request->accept_socket;
            const int opt_rs = ::setsockopt(
                accept_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                (char*) &listening_socket, sizeof(listening_socket));
            if (0 != opt_rs)
            {
                const int errcode = ::WSAGetLastError();
                if (0 != ::closesocket(accept_socket))
                    LOOFAH_LOG_FD_ERRNO(closesocket, accept_socket);
                handler->handle_io_error(from_errno(errcode));
                break;
            }

            handler->handle_accept_completed(accept_socket);
            break;
        }

        case ProactHandler::CONNECT_MASK:
        {
            // 通过 ConnectEx() 连接的 socket，还需要用 SO_UPDATE_ACCEPT_CONTEXT
            // 初始化一下
            const socket_t fd = handler->get_socket();
            const int opt_rs = ::setsockopt(
                fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) &fd, sizeof(fd));
            if (0 != opt_rs)
            {
                LOOFAH_LOG_FD_ERRNO(setsockopt, fd);
                handler->handle_io_error(from_errno(::WSAGetLastError()));
                break;
            }

            handler->handle_connect_completed();
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
            assert(false);
        }

        IORequest::delete_request(io_request);
    }
#elif NUT_PLATFORM_OS_MACOS
    struct timespec timeout;
    if (timeout_ms < 0)
    {
        timeout.tv_sec = 31536000; // NOTE 太大的数会溢出导致无法正常工作
        timeout.tv_nsec = 999999999;
    }
    else
    {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
    }
    struct kevent active_evs[LOOFAH_MAX_ACTIVE_EVENTS];

    _poll_stage = PollStage::PollingWait;
    const int n = ::kevent(_kq, nullptr, 0, active_evs, LOOFAH_MAX_ACTIVE_EVENTS, &timeout);
    _poll_stage = PollStage::HandlingEvents;
    for (int i = 0; i < n; ++i)
    {
        // User event
        const int filter = active_evs[i].filter;
        if (EVFILT_USER == filter)
        {
            assert(KQUEUE_WAKEUP_IDENT == active_evs[i].ident &&
                   nullptr == active_evs[i].udata);
            continue;
        }

        // Socket events
        ProactHandler *handler = (ProactHandler*) active_evs[i].udata;
        assert(nullptr != handler);
        const socket_t fd = handler->get_socket();

        if (EVFILT_READ == filter)
        {
            if (0 != (handler->_enabled_events & ProactHandler::ACCEPT_MASK))
            {
                assert(handler->_request_accept > 0);
                --handler->_request_accept;
                if (0 == handler->_request_accept)
                    disable_handler(handler, ProactHandler::ACCEPT_MASK);

                while (true)
                {
                    const socket_t accepted = ReactAcceptorBase::accept(fd);
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
                    handler->handle_io_error(from_errno(errno));

                IORequest::delete_request(io_request);
            }
        }
        else if (EVFILT_WRITE == filter)
        {
            if (0 != (handler->_enabled_events & ProactHandler::CONNECT_MASK))
            {
                const int errcode = SockOperation::get_last_error(fd);
                disable_handler(handler, ProactHandler::CONNECT_MASK);
                if (0 == errcode)
                    handler->handle_connect_completed();
                else
                    handler->handle_io_error(errcode);
            }
            else
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
                    handler->handle_io_error(from_errno(errno));

                IORequest::delete_request(io_request);
            }
        }
        else
        {
            NUT_LOG_E(TAG, "unknown kevent filter type %d", filter);
            return -1;
        }
    }
#elif NUT_PLATFORM_OS_LINUX
    const int timeout = (timeout_ms < 0 ? -1 : timeout_ms);
    struct epoll_event events[LOOFAH_MAX_ACTIVE_EVENTS];
    _poll_stage = PollStage::PollingWait;
    const int n = ::epoll_wait(_epoll_fd, events, LOOFAH_MAX_ACTIVE_EVENTS, timeout);
    _poll_stage = PollStage::HandlingEvents;
    for (int i = 0; i < n; ++i)
    {
        // 'eventfd' events
        if (events[i].data.fd == _event_fd)
        {
            if (0 != (events[i].events & EPOLLERR))
            {
                NUT_LOG_W(TAG, "eventfd error, fd %d", _event_fd);
            }
            else if (0 != (events[i].events & EPOLLIN))
            {
                uint64_t counter = 0;
                const int rs = ::read(_event_fd, &counter, sizeof(counter));
                if (rs < 0)
                    LOOFAH_LOG_FD_ERRNO(read, _event_fd);
            }

            continue;
        }

        // Socket events
        ProactHandler *handler = (ProactHandler*) events[i].data.ptr;
        assert(nullptr != handler);
        const socket_t fd = handler->get_socket();
        if (0 != (events[i].events & EPOLLERR))
        {
            const int errcode = SockOperation::get_last_error(fd);
            handler->handle_io_error(from_errno(errcode));
            continue;
        }

        // NOTE 可能既有 EPOLLIN 事件, 又有 EPOLLOUT 事件
        if (0 != (events[i].events & EPOLLIN))
        {
            if (0 != (handler->_enabled_events & ProactHandler::ACCEPT_MASK))
            {
                assert(handler->_request_accept > 0);
                --handler->_request_accept;
                if (0 == handler->_request_accept)
                    disable_handler(handler, ProactHandler::ACCEPT_MASK);

                while (true)
                {
                    const socket_t accepted = ReactAcceptorBase::accept(fd);
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
                    handler->handle_io_error(from_errno(errno));

                IORequest::delete_request(io_request);
            }
        }
        if (0 != (events[i].events & EPOLLOUT))
        {
            if (0 != (handler->_enabled_events & ProactHandler::CONNECT_MASK))
            {
                const int errcode = SockOperation::get_last_error(fd);
                disable_handler(handler, ProactHandler::CONNECT_MASK);
                if (0 == errcode)
                    handler->handle_connect_completed();
                else
                    handler->handle_io_error(errcode);
            }
            else
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
                    handler->handle_io_error(from_errno(errno));

                IORequest::delete_request(io_request);
            }
        }
    }
#endif

    // Run asynchronized tasks
    _poll_stage = PollStage::NotPolling;
    run_later_tasks();

    return 0;
}

void Proactor::wakeup_poll_wait() noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    if (!::PostQueuedCompletionStatus(_iocp, 0, 0, nullptr))
        NUT_LOG_E(TAG, "failed to call ::PostQueuedCompletionStatus() with ::GetLastError() %d", ::GetLastError());
#elif NUT_PLATFORM_OS_MACOS
    struct kevent ev;
    EV_SET(&ev, KQUEUE_WAKEUP_IDENT, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    if (0 != ::kevent(_kq, &ev, 1, nullptr, 0, nullptr))
        LOOFAH_LOG_FD_ERRNO(kevent, _kq);
#elif NUT_PLATFORM_OS_LINUX
    const uint64_t counter = 1;
    const int rs = ::write(_event_fd, &counter, sizeof(counter));
    if (rs < 0)
        LOOFAH_LOG_FD_ERRNO(write, _event_fd);
#endif
}

}
