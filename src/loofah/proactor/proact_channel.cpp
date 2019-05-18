
#include "../loofah_config.h"

#include <assert.h>

#include "proact_channel.h"
#include "proactor.h"


namespace loofah
{

SockStream& ProactChannel::get_sock_stream()
{
    return _sock_stream;
}

void ProactChannel::open(socket_t fd)
{
    _sock_stream.open(fd);
}

socket_t ProactChannel::get_socket() const
{
    return _sock_stream.get_socket();
}

void ProactChannel::handle_accept_completed(socket_t fd)
{
    UNUSED(fd);
    assert(false); // Should not run into this place
}

void ProactChannel::handle_connect_completed()
{
    _registered_proactor->unregister_handler(this);
    handle_channel_connected();
}

}
