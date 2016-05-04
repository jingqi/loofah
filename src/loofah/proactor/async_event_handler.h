
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
struct IOContext
{
    OVERLAPPED overlapped; // NOTE 必须放在首位

    AsyncEventHandler *handler = NULL;
    int mask = 0;

    int buf_len = 0;
    char *buf = NULL;

    socket_t accept_socket = INVALID_SOCKET_VALUE;

    WSABUF wsabuf;

    IOContext(AsyncEventHandler *handler_, int mask_, int buf_len_, socket_t accept_socket_ = INVALID_SOCKET_VALUE)
        : handler(handler_), mask(mask_), buf_len(buf_len_), accept_socket(accept_socket_)
    {
        assert(NULL != handler_ && buf_len_ > 0);
        ::memset(&overlapped, 0, sizeof(overlapped));
        buf = (char*) ::malloc(buf_len);
    }

    ~IOContext()
    {
        if (NULL != buf)
            ::free(buf);
        buf = NULL;
    }
};
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

    virtual void handle_accept_completed(struct IOContext*) {}
    virtual void handle_read_completed(struct IOContext*) {}
    virtual void handle_write_completed(struct IOContext*) {}
    virtual void handle_exception(struct IOContext*) {}
};

}

#endif
