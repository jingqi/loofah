
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

LOOFAH_API bool init_network();
LOOFAH_API void shutdown_network();

}

#endif
