﻿
#include <assert.h>
#include <stdlib.h>
#include <new>
#include <algorithm> // for std::max()

#include "io_request.h"


#undef max

namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
IORequest::IORequest(ProactHandler *handler_, ProactHandler::mask_type event_type_,
                     size_t buf_count_, socket_t accept_socket_) noexcept
    : handler(handler_), event_type(event_type_), accept_socket(accept_socket_),
      buf_count(buf_count_)
{
    assert(nullptr != handler_);
    assert(ProactHandler::ACCEPT_MASK == event_type_ ||
           ProactHandler::CONNECT_MASK == event_type_ ||
           ProactHandler::READ_MASK == event_type_ ||
           ProactHandler::WRITE_MASK == event_type_);
    ::memset(&overlapped, 0, sizeof(overlapped));
}
#else
IORequest::IORequest(ProactHandler::mask_type event_type_, size_t buf_count_) noexcept
    : event_type(event_type_), buf_count(buf_count_)
{
    assert(ProactHandler::READ_MASK == event_type_ ||
           ProactHandler::WRITE_MASK == event_type_);
}
#endif

#if NUT_PLATFORM_OS_WINDOWS
IORequest* IORequest::new_request(
    ProactHandler *handler, ProactHandler::mask_type event_type, size_t buf_count,
    socket_t accept_socket) noexcept
{
    assert(nullptr != handler);

    const size_t size = sizeof(IORequest) + sizeof(WSABUF) * (std::max((size_t) 1, buf_count) - 1);
    IORequest *p = (IORequest*) ::malloc(size);
    assert(nullptr != p);
    new (p) IORequest(handler, event_type, buf_count, accept_socket);
    return p;
}
#else
IORequest* IORequest::new_request(ProactHandler::mask_type event_type, size_t buf_count) noexcept
{
    const size_t size = sizeof(IORequest) + sizeof(struct iovec) * (std::max((size_t) 1, buf_count) - 1);
    IORequest *p = (IORequest*) ::malloc(size);
    assert(nullptr != p);
    new (p) IORequest(event_type, buf_count);
    return p;
}
#endif

void IORequest::delete_request(IORequest *p) noexcept
{
    assert(nullptr != p);
    p->~IORequest();
    ::free(p);
}

void IORequest::set_buf(size_t index, void *buf, size_t len) noexcept
{
    assert(index < buf_count);

#if NUT_PLATFORM_OS_WINDOWS
    wsabufs[index].buf = (char*) buf;
    wsabufs[index].len = len;
#else
    iovs[index].iov_base = (char*) buf;
    iovs[index].iov_len = len;
#endif
}

void IORequest::set_bufs(void* const *buf_ptrs, const size_t *len_ptrs) noexcept
{
    assert(nullptr != buf_ptrs && nullptr != len_ptrs);

    for (size_t i = 0; i < buf_count; ++i)
    {
#if NUT_PLATFORM_OS_WINDOWS
        wsabufs[i].buf = (char*) buf_ptrs[i];
        wsabufs[i].len = len_ptrs[i];
#else
        iovs[i].iov_base = (char*) buf_ptrs[i];
        iovs[i].iov_len = len_ptrs[i];
#endif
    }
}

}
