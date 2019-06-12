
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#   include <mswsock.h>
#else
#   include <sys/socket.h> // for setsockopt() and so on
#   include <fcntl.h> // for fcntl()
#endif

#include <nut/logging/logger.h>

#include "utils.h"
#include "error.h"


#define TAG "loofah.inet_base.utils"

namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
LPFN_ACCEPTEX func_AcceptEx = nullptr;
LPFN_CONNECTEX func_ConnectEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS func_GetAcceptExSockaddrs = nullptr;
#endif

bool init_network() noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    // 初始化网络库
    WSADATA wsa;
    int rs = ::WSAStartup(MAKEWORD(2,1), &wsa);
    if (0 != rs)
    {
        // NOTE 'rs' is errno, don't call ::WSAGetLastError()
        NUT_LOG_E(TAG, "failed to call WSAStartup() with return %d", rs);
        return false;
    }

    // 获取几个 API 函数指针
    SOCKET proxy_socket = ::socket(AF_INET, SOCK_STREAM, 0); // NOTE 只需要一个有效的 socket 即可，该 socket 的类型不影响什么
    if (INVALID_SOCKET == proxy_socket)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }
    GUID func_guid = WSAID_ACCEPTEX;
    DWORD bytes = 0;
    rs = ::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
                    &func_AcceptEx, sizeof(func_AcceptEx), &bytes, nullptr, nullptr);
    if (0 != rs || nullptr == func_AcceptEx)
    {
        LOOFAH_LOG_ERRNO(WSAIoctl);
        ::closesocket(proxy_socket);
        return false;
    }

    func_guid = WSAID_CONNECTEX;
    bytes = 0;
    rs = ::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
                    &func_ConnectEx, sizeof(func_ConnectEx), &bytes, nullptr, nullptr);
    if (0 != rs || nullptr == func_ConnectEx)
    {
        LOOFAH_LOG_ERRNO(WSAIoctl);
        ::closesocket(proxy_socket);
        return false;
    }

    func_guid = WSAID_GETACCEPTEXSOCKADDRS;
    bytes = 0;
    rs = ::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
                    &func_GetAcceptExSockaddrs, sizeof(func_GetAcceptExSockaddrs), &bytes, nullptr, nullptr);
    if (0 != rs || nullptr == func_GetAcceptExSockaddrs)
    {
        LOOFAH_LOG_ERRNO(WSAIoctl);
        ::closesocket(proxy_socket);
        return false;
    }

    if (0 != ::closesocket(proxy_socket))
        LOOFAH_LOG_ERRNO(closesocket);

    return true;
#else
    return true;
#endif
}

void shutdown_network() noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    if (0 != ::WSACleanup())
        LOOFAH_LOG_ERRNO(WSACleanup);
#endif
}

}
