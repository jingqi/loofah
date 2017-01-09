﻿
#include <loofah/loofah.h>
#include <nut/nut.h>

#if !NUT_PLATFORM_OS_WINDOWS
#   include <unistd.h>
#endif


#define TAG "test_proactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2346
#define BUF_LEN 256

using namespace nut;
using namespace loofah;

namespace
{

Proactor g_proactor;
class ServerChannel;
std::vector<rc_ptr<ServerChannel> > g_server_channels;

class ServerChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void initialize() override
    {
        g_server_channels.push_back(this);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server channel connected");

        g_proactor.async_register_handler(this);

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.async_launch_read(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from client: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            g_proactor.async_unregister_handler(this);
            _sock_stream.close();
            g_proactor.async_shutdown();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.async_launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to client: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.async_launch_read(this, &buf, &len, 1);
    }
};

class ClientChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void initialize() override
    {}

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client channel connected");

        g_proactor.async_register_handler(this);

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.async_launch_write(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from server: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            g_proactor.async_unregister_handler(this);
            _sock_stream.close();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            g_proactor.async_unregister_handler(this);
            _sock_stream.close();
            return;
        }

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.async_launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to server: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.async_launch_read(this, &buf, &len, 1);
    }
};

}

void test_proactor()
{
    // start server
    rc_ptr<ProactAcceptor<ServerChannel> > acc = rc_new<ProactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc->open(addr);
    g_proactor.async_register_handler(acc);
    g_proactor.async_launch_accept(acc);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());

    // start client
    rc_ptr<ClientChannel> client = ProactConnector<ClientChannel>::connect(addr);
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());

    // loop
    while (true)
    {
        if (g_proactor.handle_events() < 0)
            break;
    }
    g_server_channels.clear();
    g_proactor.async_shutdown();
}