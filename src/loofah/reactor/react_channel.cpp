
#include "../loofah_config.h"

#include <nut/platform/platform.h>
#include <nut/logging/logger.h>

#include "react_channel.h"


#define TAG "react_channel"

namespace loofah
{

void ReactChannel::open(socket_t fd)
{
    _sock_stream.open(fd);
}

socket_t ReactChannel::get_socket() const
{
    return _sock_stream.get_socket();
}

SockStream& ReactChannel::get_sock_stream()
{
    return _sock_stream;
}

}
