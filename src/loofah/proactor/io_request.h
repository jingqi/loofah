
#ifndef ___HEADFILE_9FF20425_5A52_4AF1_A64E_9847FC8625B7_
#define ___HEADFILE_9FF20425_5A52_4AF1_A64E_9847FC8625B7_

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
#   include <sys/uio.h> // for ::readv()
#elif NUT_PLATFORM_OS_LINUX
#   include <sys/epoll.h> // for ::epoll_create()
#   include <unistd.h> // for ::close()
#   include <errno.h>
#   include <sys/uio.h> // for ::readv()
#endif

#include <string.h>


namespace loofah
{

class IORequest
{
public:
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE OVERLAPPED 必须放在首位
    OVERLAPPED overlapped;

    // 事件类型
    int event_type = 0;

    socket_t accept_socket = INVALID_SOCKET_VALUE;

    const size_t buf_count = 0;

    // NOTE 这一部分是变长的，应该作为最后一个成员
    WSABUF wsabufs[1];
#else
    // 事件类型
    int event_type = 0;

    const size_t buf_count = 0;

    // NOTE 这一部分是变长的，应该作为最后一个成员
    struct iovec iovs[1];
#endif

private:
#if NUT_PLATFORM_OS_WINDOWS
    IORequest(int event_type_, size_t buf_count_, socket_t accept_socket_);
#else
    IORequest(int event_type_, size_t buf_count_);
#endif

    IORequest(const IORequest&);
    IORequest& operator=(const IORequest&);

    ~IORequest();

public:
#if NUT_PLATFORM_OS_WINDOWS
    static IORequest* new_request(int event_type, size_t buf_count,
                                  socket_t accept_socket = INVALID_SOCKET_VALUE);
#else
    static IORequest* new_request(int event_type, size_t buf_count);
#endif

    static void delete_request(IORequest *p);

    void set_buf(size_t index, void *buf, size_t len);
    void set_bufs(void* const *buf_ptrs, const size_t *len_ptrs);
};

}

#endif
