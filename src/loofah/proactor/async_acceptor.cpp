
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#endif

#include <nut/logging/logger.h>

#include "async_acceptor.h"
#include "proactor.h"
#include "../utils.h"

#define TAG "loofah.async_acceptor"

namespace loofah
{

bool AsyncAcceptorBase::open(int port, int listen_num)
{
#if NUT_PLATFORM_OS_WINDOWS
    // create socket
    _listen_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET_VALUE == _listen_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::WSASocket()");
        return false;
    }

    // bind
    struct sockaddr_in sin;
    ::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    if (SOCKET_ERROR == ::bind(_listen_socket, (struct sockaddr*)&sin, sizeof(sin)))
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with port %d", port);
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // listen
    if (SOCKET_ERROR == ::listen(_listen_socket, listen_num))
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with port %d", port);
        ::closesocket(_listen_socket);
        _listen_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    return true;
#endif
}

void AsyncAcceptorBase::associate_proactor(Proactor *proactor)
{
    assert(NULL != proactor);
    assert(INVALID_SOCKET_VALUE != _listen_socket && NULL == _proactor);

#if NUT_PLATFORM_OS_WINDOWS
    const HANDLE rs_iocp = ::CreateIoCompletionPort((HANDLE)_listen_socket, proactor->get_iocp(), 0, 0);
    if (NULL == rs_iocp)
    {
        NUT_LOG_E(TAG, "failed to associate IOCP with errno %d", ::GetLastError());
        return;
    }
    _proactor = proactor;
#endif
}

void AsyncAcceptorBase::launch_accept()
{
    assert(INVALID_SOCKET_VALUE != _listen_socket && NULL != _proactor);

#if NUT_PLATFORM_OS_WINDOWS
    socket_t accept_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET_VALUE == accept_socket)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return;
    }

    struct IOContext io_context(this, AsyncEventHandler::ACCEPT_MASK, 2 * (sizeof(struct sockaddr_in) + 16), accept_socket);
    DWORD bytes = 0;
    assert(NULL != func_AcceptEx);
    const BOOL rs = func_AcceptEx(_listen_socket,
                                  accept_socket,
                                  io_context.buf,
                                  io_context.buf_len - 2 * (sizeof(struct sockaddr_in) + 16), // substract address length
                                  sizeof(struct sockaddr_in) + 16, // local address length
                                  sizeof(struct sockaddr_in) + 16, // remote address length
                                  &bytes, (LPOVERLAPPED)&io_context);
    if (FALSE == rs)
    {
        NUT_LOG_E(TAG, "failed to call ::AcceptEx()");
        ::closesocket(accept_socket);
        return;
    }
    HANDLE rs_iocp = ::CreateIoCompletionPort((HANDLE)accept_socket, _proactor->get_iocp(), 0, 0);
    if (NULL == rs_iocp)
    {
        NUT_LOG_E(TAG, "failed to associate IOCP with errno %d", ::GetLastError());
        ::closesocket(accept_socket);
        return;
    }
#endif
}

}
