﻿
#include "../loofah_config.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "error.h"


#define TAG "loofah.inet_base.error"

#define CASE_MAP(sys_err, loofah_err) case sys_err: return loofah_err

namespace loofah
{

int from_errno(int err) noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    // Check https://docs.microsoft.com/zh-cn/windows/desktop/WinSock/windows-sockets-error-codes-2
    switch (err)
    {
        CASE_MAP(WSA_INVALID_HANDLE, LOOFAH_ERR_INVALID_FD);
        CASE_MAP(WSAEWOULDBLOCK, LOOFAH_ERR_WOULD_BLOCK);
        CASE_MAP(WSAENOTCONN, LOOFAH_ERR_NOT_CONNECTED);
        CASE_MAP(WSAECONNRESET, LOOFAH_ERR_CONNECTION_RESET);
        CASE_MAP(WSAECONNABORTED, LOOFAH_ERR_CONNECTION_ABORTED);
    }
    NUT_LOG_W(TAG, "unmanaged error for errno %d", err);
#else
    switch (err)
    {
        CASE_MAP(EAGAIN, LOOFAH_ERR_WOULD_BLOCK);
#   if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        CASE_MAP(EWOULDBLOCK, LOOFAH_ERR_WOULD_BLOCK);
#   endif
        CASE_MAP(ENOTCONN, LOOFAH_ERR_NOT_CONNECTED);
        CASE_MAP(ECONNRESET, LOOFAH_ERR_CONNECTION_RESET);
        CASE_MAP(ECONNABORTED, LOOFAH_ERR_CONNECTION_ABORTED);
        CASE_MAP(EPIPE, LOOFAH_ERR_BROKEN_PIPE);
    }
    NUT_LOG_W(TAG, "unmanaged error for errno %d: %s", err, ::strerror(err));
#endif

    return LOOFAH_ERR_UNKNOWN;
}

LOOFAH_API const char* str_error(int err) noexcept
{
    switch (err)
    {
        CASE_MAP(0, "No error");
        CASE_MAP(LOOFAH_ERR_UNKNOWN, "Unknown error");
#if NUT_PLATFORM_OS_WINDOWS
        CASE_MAP(LOOFAH_ERR_INVALID_FD, "Invalid handle");
#else
        CASE_MAP(LOOFAH_ERR_INVALID_FD, "Invalid file descriptor");
#endif
        CASE_MAP(LOOFAH_ERR_WOULD_BLOCK, "Resource temporarily unavailable, operation would bolck");
        CASE_MAP(LOOFAH_ERR_NOT_CONNECTED, "Socket is not connected");
        CASE_MAP(LOOFAH_ERR_CONNECTION_RESET, "Connection reset by peer");
        CASE_MAP(LOOFAH_ERR_CONNECTION_ABORTED, "Software caused connection abort");
        CASE_MAP(LOOFAH_ERR_BROKEN_PIPE, "Broken pipe");
        CASE_MAP(LOOFAH_ERR_PKG_OVERSIZE, "Package payload size is too big");
        CASE_MAP(LOOFAH_ERR_TIMEOUT, "Channel wait timeout");
    }
    return "Undefined error";
}

}
