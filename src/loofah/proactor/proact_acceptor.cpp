﻿
#include <assert.h>
#include <new>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <unistd.h> // for ::close()
#endif

#include <nut/logging/logger.h>

#include "proact_acceptor.h"
#include "../base/utils.h"
#include "../base/sock_base.h"


#define TAG "loofah.proact_acceptor"

namespace loofah
{

bool ProactAcceptorBase::open(const INETAddr& addr, int listen_num)
{
    // Create socket
#if NUT_PLATFORM_OS_WINDOWS
    // NOTE 必须使用 ::WSASocket() 创建 socket, 并带上 WSA_FLAG_OVERLAPPED 标记，
    //      以便用于 iocp
    _listener_socket = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
    _listener_socket = ::socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (INVALID_SOCKET_VALUE == _listener_socket)
    {
#if NUT_PLATFORM_OS_WINDOWS
        NUT_LOG_E(TAG, "failed to call ::WSASocket()");
#else
        NUT_LOG_E(TAG, "failed to call ::socket()");
#endif
        return false;
    }

    // Make port reuseable
    if (!SockBase::make_listen_addr_reuseable(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket reuseable, socketfd %d", _listener_socket);

    // Bind
    const struct sockaddr_in& sin = addr.get_sockaddr_in();
    // sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(INADDR_ANY);
    // sin.sin_port = htons(port);
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == ::bind(_listener_socket, (const struct sockaddr*)&sin, sizeof(sin)))
#else
    if (::bind(_listener_socket, (const struct sockaddr*)&sin, sizeof(sin)) < 0)
#endif
    {
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", addr.to_string().c_str());
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listener_socket);
#else
        ::close(_listener_socket);
#endif
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Listen
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == ::listen(_listener_socket, listen_num))
#else
    if (::listen(_listener_socket, listen_num) < 0)
#endif
    {
        NUT_LOG_E(TAG, "failed to call ::listen() with addr %s", addr.to_string().c_str());
#if NUT_PLATFORM_OS_WINDOWS
        ::closesocket(_listener_socket);
#else
        ::close(_listener_socket);
#endif
        _listener_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // Make socket non-blocking
    if (!SockBase::make_nonblocking(_listener_socket))
        NUT_LOG_W(TAG, "failed to make listen socket nonblocking, socketfd %d", _listener_socket);

    return true;
}

}
