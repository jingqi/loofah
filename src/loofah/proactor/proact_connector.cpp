
#include "../loofah_config.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "proact_connector.h"
#include "proact_channel.h"
#include "proactor.h"
#include "../inet_base/utils.h"
#include "../inet_base/sock_operation.h"
#include "../inet_base/error.h"


#define TAG "loofah.proact_connector"

namespace loofah
{

bool ProactConnectorBase::connect(Proactor *proactor, const InetAddr& address)
{
    assert(nullptr != proactor);

    // New socket
#if NUT_PLATFORM_OS_WINDOWS
    const int domain = address.is_ipv6() ? AF_INET6 : AF_INET;
    const socket_t fd = ::WSASocket(domain, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (LOOFAH_INVALID_SOCKET_FD == fd)
    {
        LOOFAH_LOG_ERRNO(WSASocket);
        return false;
    }
#else
    const int domain = address.is_ipv6() ? PF_INET6 : PF_INET;
    const socket_t fd = ::socket(domain, SOCK_STREAM, 0);
    if (LOOFAH_INVALID_SOCKET_FD == fd)
    {
        LOOFAH_LOG_ERRNO(socket);
        return false;
    }
#endif

    // Make it nonblocking
    if (!SockOperation::set_nonblocking(fd))
        NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

#if NUT_PLATFORM_OS_WINDOWS
    // IOCP need binding socket first!
    InetAddr local_addr(0, false, address.is_ipv6()); // port 0 on INADDR_ANY
    if (::bind(fd, local_addr.cast_to_sockaddr(), local_addr.get_sockaddr_size()) < 0)
    {
        LOOFAH_LOG_FD_ERRNO(bind, fd);
        NUT_LOG_E(TAG, "failed to call ::bind() with addr %s", local_addr.to_string().c_str());
        SockOperation::close(fd);
        return false;
    }

    // Register to proactor
    nut::rc_ptr<ProactChannel> channel = create_channel();
    channel->initialize();
    channel->open(fd);
    proactor->register_handler_later(channel);
    proactor->launch_connect_later(channel, address);
    return true;
#else
    // Connect
    const int status = ::connect(fd, address.cast_to_sockaddr(), address.get_sockaddr_size());
    if (-1 == status)
    {
        if (EINPROGRESS == errno) // 异步完成 connect()
        {
            nut::rc_ptr<ProactChannel> channel = create_channel();
            channel->initialize();
            channel->open(fd);
            proactor->register_handler_later(channel);
            proactor->launch_connect_later(channel);
            return true;
        }

        LOOFAH_LOG_FD_ERRNO(connect, fd);
        SockOperation::close(fd);
        return false;
    }

    nut::rc_ptr<ProactChannel> channel = create_channel();
    channel->initialize();
    channel->open(fd);
    channel->handle_channel_connected();
    return true;
#endif
}

}
