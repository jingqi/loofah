﻿
#include "../loofah_config.h"

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <winsock2.h>
#   include <windows.h>
#else
#   include <sys/socket.h> // for ::socket() and so on
#   include <netinet/in.h> // for struct sockaddr_in
#   include <errno.h>
#endif

#include <nut/logging/logger.h>

#include "connector_base.h"
#include "utils.h"
#include "sock_operation.h"
#include "error.h"


#define TAG "loofah.inet_base.connector_base"

namespace loofah
{

bool ConnectorBase::connect(Channel *channel, const InetAddr& address)
{
    assert(nullptr != channel);

    // New socket
    const int domain = address.is_ipv6() ? PF_INET6 : PF_INET;
    socket_t fd = ::socket(domain, SOCK_STREAM, 0);
    if (LOOFAH_INVALID_SOCKET_FD == fd)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }

    // Connect
    const int status = ::connect(fd, address.cast_to_sockaddr(), address.get_sockaddr_size());
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == status)
#else
    if (-1 == status)
#endif
    {
        LOOFAH_LOG_ERRNO(connect);
        const int save_errno = errno;
        SockOperation::close(fd);
        errno = save_errno; // The SockOperation::shutdown() may set new errno
        return false;
    }

    // Make it nonblocking
    if (!SockOperation::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    channel->open(fd);
    channel->handle_connected();

    return true;
}

}
