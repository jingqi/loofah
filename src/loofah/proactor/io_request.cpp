
#include <assert.h>
#include <stdlib.h>
#include <new>

#include "io_request.h"


namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
IORequest::IORequest(int event_type_, size_t buf_count_, socket_t accept_socket_)
    : event_type(event_type_), accept_socket(accept_socket_),
      buf_count(buf_count_)
{
    assert(buf_count_ > 0);
    ::memset(&overlapped, 0, sizeof(overlapped));
}
#else
IORequest::IORequest(int event_type_, size_t buf_count_)
    : event_type(event_type_), buf_count(buf_count_)
{
    assert(buf_count_ > 0);
}
#endif

#if NUT_PLATFORM_OS_WINDOWS
IORequest* IORequest::new_request(int event_type, size_t buf_count,
                                  socket_t accept_socket)
{
    assert(buf_count > 0);
    IORequest *p = (IORequest*) ::malloc(sizeof(IORequest) +
                                         sizeof(WSABUF) * (buf_count - 1));
    assert(nullptr != p);
    new (p) IORequest(event_type, buf_count, accept_socket);
    return p;
}
#else
IORequest* IORequest::new_request(int event_type, size_t buf_count)
{
    assert(buf_count > 0);
    IORequest *p = (IORequest*) ::malloc(sizeof(IORequest) +
                                         sizeof(struct iovec) * (buf_count - 1));
    assert(nullptr != p);
    new (p) IORequest(event_type, buf_count);
    return p;
}
#endif

void IORequest::delete_request(IORequest *p)
{
    assert(nullptr != p);
    p->~IORequest();
    ::free(p);
}

void IORequest::set_buf(size_t index, void *buf, size_t len)
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

void IORequest::set_bufs(void* const *buf_ptrs, const size_t *len_ptrs)
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
