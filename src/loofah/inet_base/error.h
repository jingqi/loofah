﻿
#ifndef ___HEADFILE_4AE79154_FD6F_4764_AA01_F44F7183F1A8_
#define ___HEADFILE_4AE79154_FD6F_4764_AA01_F44F7183F1A8_

#include "../loofah_config.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_MACOS || NUT_PLATFORM_OS_LINUX
#   include <errno.h>
#   include <string.h> // for ::strerror()
#endif

/**
 * NOTE 所有错误值都是负数
 */

// 未知错误
#define LOOFAH_ERR_UNKNOWN -1

// 无效 socket fd
#define LOOFAH_ERR_INVALID_FD -2

// non-block 操作不能立即完成，会 block
#define LOOFAH_ERR_WOULD_BLOCK -3

// socket 未连接完成
#define LOOFAH_ERR_NOT_CONNECTED -4

// 连接被拒绝
#define LOOFAH_ERR_CONN_REFUSED -5

// 连接被重置
// NOTE 对端发送了 RST 包
#define LOOFAH_ERR_CONNECTION_RESET -6

// socket 被放弃
// NOTE 对端发送了 FIN 之后又发送了 RST 包
#define LOOFAH_ERR_CONNECTION_ABORTED -7

// linux 上收到 RST 包之后，继续 write 会触发该错误，并且会收到 SIGPIPE 信号
#define LOOFAH_ERR_BROKEN_PIPE -8

// package 大小超限
#define LOOFAH_ERR_PKG_OVERSIZE -9

// 超时
#define LOOFAH_ERR_TIMEOUT -10


// logging errno
#if NUT_PLATFORM_OS_WINDOWS

#define LOOFAH_LOG_ERR(func)                                            \
    do                                                                  \
    {                                                                   \
        const int ___errcode = ::WSAGetLastError();                     \
        char *___errstr = nullptr;                                      \
        ::FormatMessageA(                                               \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, \
            nullptr, ___errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
            (LPSTR)&___errstr, 0, nullptr);                             \
        NUT_LOG_E(TAG, "failed to call " #func "() with "               \
                  "errno %d: %s", ___errcode, ___errstr);               \
        ::LocalFree(___errstr);                                         \
    } while (false)

#define LOOFAH_LOG_ERR_CODE(func, errcode)                              \
    do                                                                  \
    {                                                                   \
        const int ___errcode = (errcode);                               \
        char *___errstr = nullptr;                                      \
        ::FormatMessageA(                                               \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, \
            nullptr, ___errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
            (LPSTR)&___errstr, 0, nullptr);                             \
        NUT_LOG_E(TAG, "failed to call " #func "() with "               \
                  "errno %d: %s", ___errcode, ___errstr);               \
        ::LocalFree(___errstr);                                         \
    } while (false)

#define LOOFAH_LOG_ERR_FD(func, fd)                                     \
    do                                                                  \
    {                                                                   \
        const int ___errcode = ::WSAGetLastError();                     \
        char *___errstr = nullptr;                                      \
        ::FormatMessageA(                                               \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, \
            nullptr, ___errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
            (LPSTR)&___errstr, 0, nullptr);                             \
        NUT_LOG_E(TAG, "failed to call " #func "() with fd %d, "        \
                  "errno %d: %s", (fd), ___errcode, ___errstr);         \
        ::LocalFree(___errstr);                                         \
    } while (false)

#define LOOFAH_LOG_ERR_FD_CODE(func, fd, errcode)                       \
    do                                                                  \
    {                                                                   \
        const int ___errcode = (errcode);                               \
        char *___errstr = nullptr;                                      \
        ::FormatMessageA(                                               \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, \
            nullptr, ___errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
            (LPSTR)&___errstr, 0, nullptr);                             \
        NUT_LOG_E(TAG, "failed to call " #func "() with fd %d, "        \
                  "errno %d: %s", (fd), ___errcode, ___errstr);         \
        ::LocalFree(___errstr);                                         \
    } while (false)

#else /* NUT_PLATFORM_OS_WINDOWS */

#define LOOFAH_LOG_ERR(func)                                           \
    do{                                                                \
        NUT_LOG_E(TAG, "failed to call " #func "() with errno %d: %s", \
                  errno, ::strerror(errno));                           \
    } while (false)

#define LOOFAH_LOG_ERR_FD(func, fd)                                 \
    do{                                                             \
        NUT_LOG_E(TAG, "failed to call " #func "() with fd %d, "    \
                  "errno %d: %s", (fd), errno, ::strerror(errno));  \
    } while (false)

#endif /* NUT_PLATFORM_OS_WINDOWS */


namespace loofah
{

/**
 * 将 Mac / Linux 下的 errno 或者 Windows 下的 GetLastError() / WSAGetLastError()
 * 返回值转换为 loofah error
 */
int from_errno(int err) noexcept;

/**
 * loofah error 字符串描述
 */
LOOFAH_API const char* str_error(int err) noexcept;

}

#endif
