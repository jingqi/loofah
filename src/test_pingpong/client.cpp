
#include <loofah/proactor/proact_channel.h>
#include <loofah/proactor/proact_connector.h>

#include <nut/logging/logger.h>

#include "declares.h"

#define TAG "pingpong.client"

namespace
{

class ClientChannel;
std::vector<rc_ptr<ClientChannel> > g_client_channels;

class ClientChannel : public ProactChannel
{
    void *_buf = NULL;

public:
    ClientChannel()
    {
        _buf = ::malloc(g_global.block_size);
    }

    ~ClientChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "client channel opened");

        g_global.proactor.register_handler(this);

        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        //NUT_LOG_D(TAG, "received %d bytes from server: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.shutdown();
            return;
        }

        assert(cb == g_global.block_size);
        ++g_global.client_read_count;
        g_global.client_read_size += cb;

        g_global.proactor.launch_write(this, &_buf, &g_global.block_size, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        assert(cb == g_global.block_size);
        g_global.proactor.launch_read(this, &_buf, &g_global.block_size, 1);
    }
};

}

void start_client()
{
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    for (int i = 0; i < g_global.connection_num; ++i)
    {
        rc_ptr<ClientChannel> client = rc_new<ClientChannel>();
        g_client_channels.push_back(client);
        ProactConnector::connect(client, addr);
        NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());
    }
}
