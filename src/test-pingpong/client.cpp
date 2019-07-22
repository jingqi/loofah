
#include <loofah/loofah.h>
#include <nut/nut.h>

#include "declares.h"


#define TAG "pingpong.client"

namespace
{

class ClientChannel;

std::vector<rc_ptr<ClientChannel>> g_client_channels;

class ClientChannel : public ProactChannel
{
    void *_buf = nullptr;

public:
    ClientChannel() noexcept
    {
        _buf = ::malloc(g_global.block_size);
        ::memset(_buf, 0x9d, g_global.block_size);
    }

    ~ClientChannel() noexcept
    {
        ::free(_buf);
    }

    virtual void initialize() noexcept override
    {
        g_client_channels.push_back(this);
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "client channel connected, fd %d", get_socket());
        g_global.proactor.register_handler(this);
        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_read_completed(size_t cb) noexcept override
    {
        if (0 == cb) // 正常结束
        {
            g_global.proactor.unregister_handler(this);
            _sock_stream.close();
            return;
        }

        if (cb != g_global.block_size)
            NUT_LOG_E(TAG, "client expect %d, but got %d received", g_global.block_size, cb);
        assert(cb == g_global.block_size);
        ++g_global.client_read_count;
        g_global.client_read_size += cb;

        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_write_completed(size_t cb) noexcept override
    {
        if (cb != g_global.block_size)
            NUT_LOG_E(TAG, "client expect %d, but got %d received", g_global.block_size, cb);
        assert(cb == g_global.block_size);
        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_io_error(int err) noexcept final override
    {
        NUT_LOG_E(TAG, "client exception %d: %s", err, str_error(err));
    }
};

}

void start_client() noexcept
{
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    ProactConnector<ClientChannel> con;
    for (int i = 0; i < g_global.connection_num; ++i)
    {
        con.connect(&g_global.proactor, addr);
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
    }
}
