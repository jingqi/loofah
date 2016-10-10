
#ifndef ___HEADFILE_F7D25C02_82B8_45D8_ACB0_C2504E27D677_
#define ___HEADFILE_F7D25C02_82B8_45D8_ACB0_C2504E27D677_

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#     include <winsock2.h>
#     include <windows.h>
#endif

namespace loofah
{

// native file descriptor or handle
#if NUT_PLATFORM_OS_WINDOWS
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
#endif

}

#endif
