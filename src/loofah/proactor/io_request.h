
#ifndef ___HEADFILE_9FF20425_5A52_4AF1_A64E_9847FC8625B7_
#define ___HEADFILE_9FF20425_5A52_4AF1_A64E_9847FC8625B7_

#include "../loofah_config.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <sys/uio.h> // for ::readv()
#endif

#include <string.h> // for size_t

#include "proact_handler.h"


namespace loofah
{

class IORequest
{
public:
#if NUT_PLATFORM_OS_WINDOWS
    static IORequest* new_request(
        ProactHandler *handler, ProactHandler::mask_type event_type,
        size_t buf_count = 0, socket_t accept_socket = LOOFAH_INVALID_SOCKET_FD);
#else
    static IORequest* new_request(ProactHandler::mask_type event_type, size_t buf_count = 0);
#endif

    static void delete_request(IORequest *p);

    void set_buf(size_t index, void *buf, size_t len);
    void set_bufs(void* const *buf_ptrs, const size_t *len_ptrs);

private:
#if NUT_PLATFORM_OS_WINDOWS
    IORequest(ProactHandler *handler, ProactHandler::mask_type event_type_,
              size_t buf_count_, socket_t accept_socket_);
#else
    IORequest(ProactHandler::mask_type event_type_, size_t buf_count_);
#endif

    IORequest(const IORequest&) = delete;
    IORequest& operator=(const IORequest&) = delete;

    ~IORequest() = default;

public:
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE OVERLAPPED 必须放在首位
    OVERLAPPED overlapped;

    // Handler
    ProactHandler *handler = nullptr;

    // 事件类型
    ProactHandler::mask_type event_type = 0;

    // 为 AcceptEx() 准备的数据
    socket_t accept_socket = LOOFAH_INVALID_SOCKET_FD;

    const size_t buf_count = 0;

    // NOTE 这一部分是变长的，应该作为最后一个成员
    WSABUF wsabufs[1];
#else
    // 事件类型
    ProactHandler::mask_type event_type = 0;

    const size_t buf_count = 0;

    // NOTE 这一部分是变长的，应该作为最后一个成员
    struct iovec iovs[1];
#endif
};

}

#endif
