
#include "../loofah_config.h"

#include <assert.h>

#include "react_channel.h"
#include "reactor.h"


namespace loofah
{

SockStream& ReactChannel::get_sock_stream() noexcept
{
    return _sock_stream;
}

void ReactChannel::open(socket_t fd) noexcept
{
    _sock_stream.open(fd);
}

socket_t ReactChannel::get_socket() const noexcept
{
    return _sock_stream.get_socket();
}

void ReactChannel::handle_accept_ready() noexcept
{
    assert(false); // Should not run into this place
}

void ReactChannel::handle_connect_ready() noexcept
{
    const int errcode = _sock_stream.get_last_error();
    if (0 != errcode)
    {
        handle_io_error(errcode);
        return;
    }

    _registered_reactor->unregister_handler(this);
    handle_channel_connected();
}

}
