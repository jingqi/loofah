
#ifndef ___HEADFILE_F7D25C02_82B8_45D8_ACB0_C2504E27D677_
#define ___HEADFILE_F7D25C02_82B8_45D8_ACB0_C2504E27D677_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h>
#   include <windows.h>
#endif


#if defined(BUILDING_LOOFAH)
#   define LOOFAH_API DLL_EXPORT
#else
#   define LOOFAH_API DLL_IMPORT
#endif

/**
 * POSIX 规定 socklen_t 需要保持与 int 等长，但是 windows 下 API 是直接使用 int
 */
#if NUT_PLATFORM_OS_WINDOWS
typedef int socklen_t;
#endif

// epoll_wait() / kevent() 单次查询最多返回事件数
#define LOOFAH_MAX_ACTIVE_EVENTS 32

// 传入 writev() / readv() 栈上数组大小(元素个数)
#define LOOFAH_STACK_BUFFER_LENGTH 64

// 读操作时的初始 package payload 大小
#define LOOFAH_INIT_READ_PKG_SIZE 1024

// 默认最大 package payload 大小
#define LOOFAH_DEFAULT_MAX_PKG_SIZE (64 * 1024 * 1024)

// 强制关闭连接延时(毫秒)
// <0 表示不强制关闭, 0 表示立即关闭, >0 表示超时强制关闭
#define LOOFAH_FORCE_CLOSE_DELAY -1

namespace loofah
{

// Invalid socket file descriptor
#if NUT_PLATFORM_OS_WINDOWS
    // ::socket() returns -1 when failed
    // ::WSASocket() returns INVALID_SOCKET when failed, which is also -1
    typedef SOCKET socket_t;
    #define LOOFAH_INVALID_SOCKET_FD INVALID_SOCKET
#else
    typedef int socket_t;
    #define LOOFAH_INVALID_SOCKET_FD -1
#endif

}

#endif
