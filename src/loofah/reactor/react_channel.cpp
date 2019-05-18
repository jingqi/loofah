
#include "../loofah_config.h"

#include <assert.h>

#include "react_channel.h"
#include "reactor.h"


namespace loofah
{

SockStream& ReactChannel::get_sock_stream()
{
    return _sock_stream;
}

void ReactChannel::open(socket_t fd)
{
    _sock_stream.open(fd);
}

socket_t ReactChannel::get_socket() const
{
    return _sock_stream.get_socket();
}

void ReactChannel::handle_accept_ready()
{
    assert(false); // Should not run into this place
}

void ReactChannel::handle_connect_ready()
{
    const int errcode = _sock_stream.get_last_error();
    _registered_reactor->unregister_handler(this);
    if (0 == errcode)
        handle_channel_connected();
    else
        handle_io_error(errcode);
}

}
