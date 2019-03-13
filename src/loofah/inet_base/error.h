
#ifndef ___HEADFILE_4AE79154_FD6F_4764_AA01_F44F7183F1A8_
#define ___HEADFILE_4AE79154_FD6F_4764_AA01_F44F7183F1A8_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
#   include <errno.h>
#   include <string.h> // for ::strerror()
#endif

/**
 * NOTE 所有错误值都是负数
 */

// 未知错误
#define LOOFAH_ERR_UNKNOWN -1

// non-block 操作不能立即完成，会 block
#define LOOFAH_ERR_WOULD_BLOCK -2

// 无效 socket fd
#define LOOFAH_ERR_INVALID_FD -3

// 连接被重置
#define LOOFAH_ERR_CONNECTION_RESET -4

// package 大小超限
#define LOOFAH_ERR_PKG_OVERSIZE -5


// logging errno
#if NUT_PLATFORM_OS_WINDOWS

#   define LOOFAH_LOG_ERRNO(func)                               \
    do                                                          \
    {                                                           \
        NUT_LOG_E(TAG, "failed to call " #func "() with "       \
                  "WSAGetLastError() %d", ::WSAGetLastError()); \
    } while (false)

#   define LOOFAH_LOG_FD_ERRNO(func, fd)                                \
    do                                                                  \
    {                                                                   \
        NUT_LOG_E(TAG, "failed to call " #func "() with fd %d, "        \
                  "WSAGetLastError() %d", (fd), ::WSAGetLastError());   \
    } while (false)

#else /* NUT_PLATFORM_OS_WINDOWS */

#   define LOOFAH_LOG_ERRNO(func)                                      \
    do{                                                                \
        NUT_LOG_E(TAG, "failed to call " #func "() with errno %d: %s", \
                  errno, ::strerror(errno));                           \
    } while (false)

#   define LOOFAH_LOG_FD_ERRNO(func, fd)                               \
    do{                                                                \
        NUT_LOG_E(TAG, "failed to call " #func "() with fd %d, "       \
                  "errno %d: %s", (fd), errno, ::strerror(errno));     \
    } while (false)

#endif /* NUT_PLATFORM_OS_WINDOWS */


namespace loofah
{

/**
 * 将 Mac/Linux 下的 errno 或者 Windows 下的 WSAGetLastError() 返回值转换为
 * loofah error
 */
int from_errno(int err);

/**
 * loofah error 字符串描述
 */
const char* str_error(int err);

}

#endif
