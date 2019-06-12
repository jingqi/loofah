
#include "../loofah_config.h"

#include <assert.h>

#include "proact_channel.h"
#include "proactor.h"


namespace loofah
{

SockStream& ProactChannel::get_sock_stream() noexcept
{
    return _sock_stream;
}

void ProactChannel::open(socket_t fd) noexcept
{
    _sock_stream.open(fd);
}

socket_t ProactChannel::get_socket() const noexcept
{
    return _sock_stream.get_socket();
}

void ProactChannel::handle_accept_completed(socket_t fd) noexcept
{
    UNUSED(fd);
    assert(false); // Should not run into this place
}

void ProactChannel::handle_connect_completed() noexcept
{
    _registered_proactor->unregister_handler(this);
    handle_channel_connected();
}

}
