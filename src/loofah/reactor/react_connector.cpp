
#include "../loofah_config.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "../inet_base/utils.h"
#include "../inet_base/sock_operation.h"
#include "../inet_base/error.h"
#include "react_connector.h"
#include "react_channel.h"
#include "reactor.h"


#define TAG "loofah.react_connector"

namespace loofah
{

bool ReactConnectorBase::connect(Reactor *reactor, const InetAddr& address) noexcept
{
    assert(nullptr != reactor);

    // New socket
    const int domain = address.is_ipv6() ? PF_INET6 : PF_INET;
    const socket_t fd = ::socket(domain, SOCK_STREAM, 0);
    if (LOOFAH_INVALID_SOCKET_FD == fd)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }

    // Make it nonblocking
    if (!SockOperation::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    // Connect
    const int status = ::connect(fd, address.cast_to_sockaddr(), address.get_sockaddr_size());
#if NUT_PLATFORM_OS_WINDOWS
    if (SOCKET_ERROR == status)
    {
        const int errcode = ::WSAGetLastError();
        if (WSAEWOULDBLOCK == errcode) // 异步完成 connect()
        {
            nut::rc_ptr<ReactChannel> channel = create_channel();
            channel->initialize();
            channel->open(fd);
            reactor->register_handler(channel, ReactHandler::CONNECT_MASK);
            return true;
        }

        LOOFAH_LOG_FD_ERRNO(connect, fd);
        SockOperation::close(fd);
        return false;
    }
#else
    if (-1 == status)
    {
        if (EINPROGRESS == errno) // 异步完成 connect()
        {
            nut::rc_ptr<ReactChannel> channel = create_channel();
            channel->initialize();
            channel->open(fd);
            reactor->register_handler(channel, ReactHandler::CONNECT_MASK);
            return true;
        }

        LOOFAH_LOG_FD_ERRNO(connect, fd);
        SockOperation::close(fd);
        return false;
    }
#endif

    nut::rc_ptr<ReactChannel> channel = create_channel();
    channel->initialize();
    channel->open(fd);
    channel->handle_channel_connected();
    return true;
}

}
