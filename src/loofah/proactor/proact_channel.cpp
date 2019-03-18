
#include <assert.h>
#include <new>

#include "proact_channel.h"


namespace loofah
{

void ProactChannel::open(socket_t fd)
{
    _sock_stream.open(fd);
}

socket_t ProactChannel::get_socket() const
{
    return _sock_stream.get_socket();
}

SockStream& ProactChannel::get_sock_stream()
{
    return _sock_stream;
}

void ProactChannel::handle_accept_completed(socket_t fd)
{
    UNUSED(fd);
    // Dummy for a channel
}

}
