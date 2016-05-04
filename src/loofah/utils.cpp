
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#   include <mswsock.h>
#else
#   include <sys/socket.h> // for setsockopt() and so on
#   include <fcntl.h> // for fcntl()
#endif

#include "utils.h"

namespace loofah
{

#if NUT_PLATFORM_OS_WINDOWS
LPFN_ACCEPTEX func_AcceptEx = NULL;
LPFN_CONNECTEX func_ConnectEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS func_GetAcceptExSockaddrs = NULL;
#endif

bool init_network()
{
#if NUT_PLATFORM_OS_WINDOWS
	// 初始化网络库
	WSADATA wsa;
	if (0 != ::WSAStartup(MAKEWORD(2,1), &wsa))
		return false;

	// 获取几个 API 函数指针
	SOCKET proxy_socket = ::socket(AF_INET, SOCK_STREAM, 0); // NOTE 只需要一个有效的 socket 即可，该 socket 的类型不影响什么
	if (INVALID_SOCKET == proxy_socket)
		return false;
	GUID func_guid = WSAID_ACCEPTEX;
	DWORD bytes = 0;
	::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
		&func_AcceptEx, sizeof(func_AcceptEx), &bytes, NULL, NULL);
	if (NULL == func_AcceptEx)
	{
		::closesocket(proxy_socket);
		return false;
	}

	func_guid = WSAID_CONNECTEX;
	bytes = 0;
	::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
		&func_ConnectEx, sizeof(func_ConnectEx), &bytes, NULL, NULL);
	if (NULL == func_ConnectEx)
	{
		::closesocket(proxy_socket);
		return false;
	}

	func_guid = WSAID_GETACCEPTEXSOCKADDRS;
	bytes = 0;
	::WSAIoctl(proxy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid),
		&func_GetAcceptExSockaddrs, sizeof(func_GetAcceptExSockaddrs), &bytes, NULL, NULL);
	if (NULL == func_GetAcceptExSockaddrs)
	{
		::closesocket(proxy_socket);
		return false;
	}

	::closesocket(proxy_socket);

	return true;
#else
	return true;
#endif
}

void shutdown_network()
{
#if NUT_PLATFORM_OS_WINDOWS
	::WSACleanup();
#endif
}

bool make_listen_socket_reuseable(socket_t listen_socket)
{
#if NUT_PLATFORM_OS_WINDOWS
    BOOL optval = TRUE;
    return 0 == ::setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
#else
    int optval = 1;
    return 0 == ::setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
#endif
}

bool make_socket_nonblocking(socket_t socket_fd, bool nonblocking)
{
#if NUT_PLATFORM_OS_WINDOWS
    unsigned long mode = (nonblocking ? 1 : 0);
    return ::ioctlsocket(socket_fd, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return ::fcntl(socket_fd, F_SETFL, flags) == 0;
#endif
}

}
