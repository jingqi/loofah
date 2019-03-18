
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
    void *_buf = nullptr;

public:
    ServerChannel()
    {
        _buf = ::malloc(g_global.block_size);
    }

    ~ServerChannel()
    {
        ::free(_buf);
    }

    virtual void initialize() final override
    {
        g_server_channels.push_back(this);
    }

    virtual void handle_channel_connected() final override
    {
        NUT_LOG_D(TAG, "server channel connected, fd %d", get_socket());
        g_global.proactor.register_handler(this);
        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_read_completed(size_t cb) final override
    {
        // NUT_LOG_D(TAG, "server received %d bytes: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            g_global.proactor.unregister_handler(this);
            _sock_stream.close();
            g_global.proactor.shutdown_later();
            return;
        }

        if (cb != g_global.block_size)
            NUT_LOG_E(TAG, "server expect %d, but got %d received", g_global.block_size, cb);
        assert(cb == g_global.block_size);
        ++g_global.server_read_count;
        g_global.server_read_size += cb;

        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_write_completed(size_t cb) final override
    {
        if (cb != g_global.block_size)
            NUT_LOG_E(TAG, "server expect %d, but got %d received", g_global.block_size, cb);
        assert(cb == g_global.block_size);
        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_io_exception(int err) final override
    {
        NUT_LOG_D(TAG, "server exception %d: %s", err, str_error(err));
    }
};

}

void start_server()
{
    g_acceptor = rc_new<ProactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    g_acceptor->listen(addr);
    g_global.proactor.register_handler_later(g_acceptor);
    g_global.proactor.launch_accept_later(g_acceptor);
    NUT_LOG_D(TAG, "server listening at %s", addr.to_string().c_str());
}
