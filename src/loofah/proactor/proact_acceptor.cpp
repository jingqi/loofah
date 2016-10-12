
#include <assert.h>
#include <new>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include <nut/logging/logger.h>

#include "proact_acceptor.h"
#include "../base/utils.h"

#define TAG "loofah.proact_acceptor"

namespace loofah
{

bool ProactAcceptorBase::open(const INETAddr& addr, int listen_num)
{
#if NUT_PLATFORM_OS_WINDOWS
    // Create socket
    // NOTE 必须使用 ::WSASocket() 创建 socket 并带上 WSA_FLAG_OVERLAPPED 标记，
    //      以便使用完成端口
    _listen_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET_VALUE == _listen_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::WSASocket()");
        return false;
    }

    // Bind
    const struct sockaddr_in& sin = addr.get_sockaddr_in();
    // sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(INADDR_ANY);
    // sin.sin_port = htons(port);
    if (SOCKET_ERROR == ::bind(_listen_socket, (const struct sockaddr*)&sin, sizeof(sin)))
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
    if (SOCKET_ERROR == ::listen(_listen_socket, listen_num))
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    return true;
#else
    assert(false);
    return false;
#endif
}

socket_t ProactAcceptorBase::handle_accept(IOContext *io_context)
{
    assert(NULL != io_context);

#if NUT_PLATFORM_OS_WINDOWS
    // Get peer address
    struct sockaddr_in *remote_addr = NULL, *local_addr = NULL;
    int remote_len = sizeof(struct sockaddr_in), local_len = sizeof(struct sockaddr_in);
    assert(NULL != func_GetAcceptExSockaddrs);
    func_GetAcceptExSockaddrs(io_context->wsabuf.buf,
                              io_context->wsabuf.len - 2 * (sizeof(struct sockaddr_in) + 16),
                              sizeof(struct sockaddr_in) + 16,
                              sizeof(struct sockaddr_in) + 16,
                              (LPSOCKADDR*)&local_addr,
                              &local_len,
                              (LPSOCKADDR*)&remote_addr,
                              &remote_len);

    return io_context->accept_socket;
#else
    assert(false);
    return INVALID_SOCKET_VALUE;
#endif
}

}
