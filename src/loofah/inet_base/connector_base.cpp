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
#include "../inet_base/utils.h"
#include "../inet_base/sock_operation.h"


#define TAG "loofah.connector_base"

namespace loofah
{

bool ConnectorBase::connect(Channel *channel, const InetAddr& address)
{
    assert(NULL != channel);

    // New socket
    const int domain = address.is_ipv6() ? PF_INET6 : PF_INET;
    socket_t fd = ::socket(domain, SOCK_STREAM, 0);
    if (-1 == fd)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // Connect
    const int status = ::connect(fd, address.cast_to_sockaddr(), address.get_sockaddr_size());
    if (-1 == status)
    {
        NUT_LOG_E(TAG, "failed to call ::connect(), socketfd %d, errno %d", fd, errno);
        int save_errno = errno;
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