
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

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor.launch_read(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from client: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.shutdown();
            proactor.shutdown();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to client: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor.launch_read(this, &buf, &len, 1);
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

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(int cb) override
    {
        NUT_LOG_D(TAG, "received %d bytes from server: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            _sock_stream.shutdown();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            _sock_stream.shutdown();
            return;
        }

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(int cb) override
    {
        NUT_LOG_D(TAG, "send %d bytes to server: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor.launch_read(this, &buf, &len, 1);
    }
};

}

void test_proactor()
{
    // start server
    ProactAcceptor<ServerChannel> acc;
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc.open(addr);
    proactor.register_handler(&acc);
    proactor.launch_accept(&acc);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());

    // start client
    ProactConnector con;
    ClientChannel client;
    con.connect(&client, addr);
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());

    // loop
    while (true)
    {
        if (proactor.handle_events() < 0)
            break;
    }
    proactor.shutdown();
}
