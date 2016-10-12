
#ifndef ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_
#define ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_

#include <assert.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include "../loofah.h"

namespace loofah
{

class AsyncEventHandler;

#if NUT_PLATFORM_OS_WINDOWS
typedef struct _IOContext
{
    // NOTE OVERLAPPED 必须放在首位
    OVERLAPPED overlapped;

    // 事件类型
    int event_type = 0;

    socket_t accept_socket = INVALID_SOCKET_VALUE;

    WSABUF wsabuf;

    _IOContext(int event_type_, int buf_len, socket_t accept_socket_ = INVALID_SOCKET_VALUE)
        : event_type(event_type_), accept_socket(accept_socket_)
    {
        ::memset(&overlapped, 0, sizeof(overlapped));

        if (buf_len >= 0)
        {
            wsabuf.buf = (char*) ::malloc(buf_len);
            assert(NULL != wsabuf.buf);
            wsabuf.len = buf_len;
        }
        else
        {
            wsabuf.buf = NULL;
            wsabuf.len = 0;
        }
    }

    ~_IOContext()
    {
        clear();
    }
    
    void clear()
    {
        if (NULL != wsabuf.buf)
            ::free(wsabuf.buf);
        wsabuf.buf = NULL;
        wsabuf.len = 0;
    }
} IOContext;
#endif

class AsyncEventHandler
{
public:
    enum EventType
    {
        ACCEPT_MASK = 1,
        READ_MASK = 1 << 1,
        WRITE_MASK = 1 << 2,
        EXCEPT_MASK = 1 << 3,
    };

    virtual ~AsyncEventHandler() {}

    virtual socket_t get_socket() const = 0;

    virtual void handle_accept_completed(IOContext *ioc) {}
    virtual void handle_read_completed(int cb, IOContext *ioc) {}
    virtual void handle_write_completed(int cb, IOContext *ioc) {}
    virtual void handle_exception(IOContext *ioc) {}
};

}

#endif
