
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
