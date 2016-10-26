
#include "declares.h"

#include <loofah/proactor/proact_channel.h>
#include <loofah/proactor/proact_acceptor.h>
#include <nut/logging/logger.h>


using namespace loofah;

#define TAG "pingpong.server"

namespace
{

class ServerChannel;
rc_ptr<ProactAcceptor<ServerChannel> > g_acceptor;
std::vector<rc_ptr<ServerChannel> > g_server_channels;

class ServerChannel : public ProactChannel
{
    void *_buf = NULL;

public:
    ServerChannel()
    {
        _buf = ::malloc(g_global.block_size);
    }
    
    ~ServerChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "server channel opened");

        g_server_channels.push_back(this);
        g_global.proactor.register_handler(this);

        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        //NUT_LOG_D(TAG, "received %d bytes from client: %d", cb, _tmp);
        if (0 == cb) // Õý³£½áÊø
        {
            _sock_stream.shutdown();
            g_global.proactor.shutdown();
            return;
        }

        assert(cb == g_global.block_size);
        ++g_global.server_read_count;

        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        assert(cb == g_global.block_size);
        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }
};

}

void start_server()
{
    g_acceptor = rc_new<ProactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    g_acceptor->open(addr);
    g_global.proactor.register_handler(g_acceptor);
    g_global.proactor.launch_accept(g_acceptor);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());
}
