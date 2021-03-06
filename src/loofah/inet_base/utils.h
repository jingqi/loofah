﻿
#ifndef ___HEADFILE_8723819F_8E41_4D8B_AB1F_81205AC4CE38_
#define ___HEADFILE_8723819F_8E41_4D8B_AB1F_81205AC4CE38_

#include "../loofah_config.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h>
#   include <windows.h>
#   include <mswsock.h>
#endif


namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
// 几个 API 函数指针，这些函数在 mswsock.lib 中
// NOTE 静态链接反而不如直接使用函数指针高效(包装函数每次调用其实都会动态查找函数指针)
extern LPFN_ACCEPTEX func_AcceptEx;
extern LPFN_CONNECTEX func_ConnectEx;
extern LPFN_GETACCEPTEXSOCKADDRS func_GetAcceptExSockaddrs;
#endif

LOOFAH_API bool init_network() noexcept;
LOOFAH_API void shutdown_network() noexcept;

#if NUT_PLATFORM_OS_WINDOWS
/**
 * 创建一对双工互连的套接字
 *
 * @param family 可选 AF_INET 或 AF_INET6
 * @param type 可选 SOCK_DGRAM 或 SOCK_STREAM
 * @param protocol 可选 IPPROTO_UDP 或 IPPROTO_TCP 或 0
 * @param fds 用于存放返回的两个套接字
 * @return 成功则返回 0
 */
LOOFAH_API int socketpair(int family, int type, int protocol, socket_t fds[2]) noexcept;
#endif

}

#endif
