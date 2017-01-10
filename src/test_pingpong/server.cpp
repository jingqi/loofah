
#include <loofah/loofah.h>
#include <nut/nut.h>

#include "declares.h"


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

    virtual void initialize() override
    {
        g_server_channels.push_back(this);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server channel connected");
        g_global.proactor.register_handler_later(this);
        g_global.proactor.launch_read_later(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        //NUT_LOG_D(TAG, "received %d bytes from client: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.close();
            g_global.proactor.shutdown_later();
            return;
        }

        assert(cb == g_global.block_size);
        ++g_global.server_read_count;
        g_global.server_read_size += cb;

        g_global.proactor.launch_write_later(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        assert(cb == g_global.block_size);
        g_global.proactor.launch_read_later(this, &_buf, &g_global.block_size, 1);
    }
};

}

void start_server()
{
    g_acceptor = rc_new<ProactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    g_acceptor->open(addr);
    g_global.proactor.register_handler_later(g_acceptor);
    g_global.proactor.launch_accept_later(g_acceptor);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());
}
