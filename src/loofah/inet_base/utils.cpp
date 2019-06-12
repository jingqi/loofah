
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
    // NOTE winsock2.h 必须放在 windows.h 之前
#   include <winsock2.h>
#   include <ws2ipdef.h> // for struct sockaddr_in6
#   include <ws2tcpip.h> // For in6addr_loopback
#   include <windows.h>
#   include <mswsock.h>
#else
#   include <sys/socket.h> // for setsockopt() and so on
#   include <netinet/in.h> // for struct sockaddr_in
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

#if NUT_PLATFORM_OS_WINDOWS

namespace
{

union SockAddr
{
    struct sockaddr_in in4; // IPv4 address
    struct sockaddr_in6 in6; // IPv6 address
};

}

static int make_loopaddr(SockAddr *addr, int family) noexcept
{
    assert(nullptr != addr);

    ::memset(addr, 0, sizeof(SockAddr));
    if (AF_INET == family)
    {
        addr->in4.sin_family = AF_INET;
        addr->in4.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        addr->in4.sin_port = 0; // NOTE 由系统随机分配一个端口
        return sizeof(struct sockaddr_in);
    }

    addr->in6.sin6_family = AF_INET6;
    addr->in6.sin6_addr = in6addr_loopback;
    addr->in6.sin6_port = 0; // NOTE 由系统随机分配一个端口
    return sizeof(struct sockaddr_in6);
}

static int stream_socketpair(int family, int type, int protocol, socket_t fds[2]) noexcept
{
    assert(nullptr != fds);

    socket_t listener = LOOFAH_INVALID_SOCKET_FD, server = LOOFAH_INVALID_SOCKET_FD,
        client = LOOFAH_INVALID_SOCKET_FD;

    do
    {
        // Listen to random port
        listener = ::socket(family, type, protocol); // Say, AF_INET, SOCK_STREAM, IPPROTO_TCP
        if (LOOFAH_INVALID_SOCKET_FD == listener)
            break;

        BOOL optval = TRUE;
        ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

        SockAddr addr;
        int addrlen = make_loopaddr(&addr, family);
        if (::bind(listener, (struct sockaddr*)&addr, addrlen) < 0)
            break;

        if (::listen(listener, 1) < 0)
            break;

        // Get listening address
        ::memset(&addr, 0, sizeof(SockAddr));
        if (0 != ::getsockname(listener, (struct sockaddr*)&addr, &addrlen))
            break;

        // Connect to listening address
        client = ::socket(family, type, protocol);
        if (LOOFAH_INVALID_SOCKET_FD == client)
            break;

        if (0 != ::connect(client, (struct sockaddr*)&addr, addrlen))
            break;

        // Accept connecting
        server = ::accept(listener, nullptr, nullptr);
        if (LOOFAH_INVALID_SOCKET_FD == server)
            break;

        // Stop listening
        ::closesocket(listener);

        // Success
        fds[0] = server;
        fds[1] = client;
        return 0;
    } while (false);

    // Failed
    if (LOOFAH_INVALID_SOCKET_FD != listener)
        ::closesocket(listener);
    if (LOOFAH_INVALID_SOCKET_FD != server)
        ::closesocket(server);
    if (LOOFAH_INVALID_SOCKET_FD != client)
        ::closesocket(client);
    fds[0] = LOOFAH_INVALID_SOCKET_FD;
    fds[1] = LOOFAH_INVALID_SOCKET_FD;
    return -1;
}

static int dgram_socketpair(int family, int type, int protocol, socket_t fds[2]) noexcept
{
    assert(nullptr != fds);

    socket_t server = LOOFAH_INVALID_SOCKET_FD, client = LOOFAH_INVALID_SOCKET_FD;

    do
    {
        // Server
        server = ::socket(family, type, protocol);
        if (LOOFAH_INVALID_SOCKET_FD == server)
            break;

        BOOL optval = TRUE;
        ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

        SockAddr server_addr;
        int server_addrlen = make_loopaddr(&server_addr, family);
        if (::bind(server, (struct sockaddr*)&server_addr, server_addrlen) < 0)
            break;

        ::memset(&server_addr, 0, sizeof(SockAddr));
        if (0 != ::getsockname(server, (struct sockaddr*)&server_addr, &server_addrlen))
            break;

        // Client
        client = ::socket(family, type, protocol);
        if (LOOFAH_INVALID_SOCKET_FD == client)
            break;

        optval = TRUE;
        ::setsockopt(client, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

        SockAddr client_addr;
        int client_addrlen = make_loopaddr(&client_addr, family);
        if (::bind(client, (struct sockaddr*)&client_addr, client_addrlen) < 0)
            break;

        ::memset(&client_addr, 0, sizeof(SockAddr));
        if (0 != ::getsockname(client, (struct sockaddr*)&client_addr, &client_addrlen))
            break;

        // Connect to each other
        if (0 != ::connect(server, (struct sockaddr*)&client_addr, client_addrlen))
            break;
        if (0 != ::connect(client, (struct sockaddr*)&server_addr, server_addrlen))
            break;

        // Success
        fds[0] = server;
        fds[1] = client;
        return 0;
    } while (false);

    // Failed
    if (LOOFAH_INVALID_SOCKET_FD != server)
        ::closesocket(server);
    if (LOOFAH_INVALID_SOCKET_FD != client)
        ::closesocket(client);
    fds[0] = LOOFAH_INVALID_SOCKET_FD;
    fds[1] = LOOFAH_INVALID_SOCKET_FD;
    return -1;
}

LOOFAH_API int socketpair(int family, int type, int protocol, socket_t fds[2]) noexcept
{
    assert(nullptr != fds);
    if (SOCK_STREAM == type)
        return stream_socketpair(family, type, protocol, fds);
    else
        return dgram_socketpair(family, type, protocol, fds);
}
#endif

}
