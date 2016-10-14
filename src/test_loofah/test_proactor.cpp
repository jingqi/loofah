
#include <loofah/proactor/proactor.h>
#include <loofah/proactor/proact_acceptor.h>
#include <loofah/proactor/proact_channel.h>
#include <loofah/proactor/proact_connector.h>

#include <nut/platform/platform.h>

#if !NUT_PLATFORM_OS_WINDOWS
#   include <unistd.h>
#endif

#include <nut/logging/logger.h>
#include <nut/threading/thread_pool.h>


#define TAG "test_proactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2346
#define BUF_LEN 256

using namespace nut;
using namespace loofah;

namespace
{

rc_ptr<ThreadPool> thread_pool;
Proactor proactor;

class ServerChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "server channel opened");

        proactor.register_handler(this);
        proactor.launch_read(this, &_tmp, sizeof(_tmp));
    }

    virtual void handle_read_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from client: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.close();
            proactor.close();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        proactor.launch_write(this, &_counter, sizeof(_counter));
    }

    virtual void handle_write_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to client: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        proactor.launch_read(this, &_tmp, sizeof(_tmp));
    }
};

class ClientChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "client channel opened");

        proactor.register_handler(this);
        proactor.launch_write(this, &_counter, sizeof(_counter));
    }

    virtual void handle_read_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from server: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.close();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            _sock_stream.close();
            return;
        }

        proactor.launch_write(this, &_counter, sizeof(_counter));
    }

    virtual void handle_write_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to server: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        proactor.launch_read(this, &_tmp, sizeof(_tmp));
    }
};

void start_proactor_server(void*)
{
    ProactAcceptor<ServerChannel> acc;
    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc.open(addr);
    proactor.register_handler(&acc);
    proactor.launch_accept(&acc);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());
    while (true)
    {
        if (proactor.handle_events() < 0)
            break;
    }
    proactor.close();
    thread_pool->interrupt();
}

void start_proactor_client(void*)
{
#if NUT_PLATFORM_OS_WINDOWS
    ::Sleep(1000);
#else
    ::sleep(1); // Wait for server to be setupped
#endif

    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    ProactConnector con;
    ClientChannel *client = (ClientChannel*) ::malloc(sizeof(ClientChannel));
    new (client) ClientChannel;
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());
    con.connect(client, addr);
}

}

void test_proactor()
{
    thread_pool = rc_new<ThreadPool>(3);
    thread_pool->add_task(&start_proactor_server);
    thread_pool->add_task(&start_proactor_client);
    thread_pool->start();
    thread_pool->join();
}
