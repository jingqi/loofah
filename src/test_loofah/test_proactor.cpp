
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
    char *_buf = NULL;
    int _sz = 0;

public:
    ServerChannel()
    {
        _buf = (char*) ::malloc(BUF_LEN);
    }

    ~ServerChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "server channel opened");

        proactor.register_handler(this);
        proactor.launch_read(this, _buf, BUF_LEN);
    }

    virtual void handle_read_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "readed %d bytes from client", cb);
        assert(cb < BUF_LEN);
        _sz = cb;

        if (_sz > 0)
            proactor.launch_write(this, _buf, _sz);
    }

    virtual void handle_write_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "wrote %d bytes to client", cb);

        proactor.launch_read(this, _buf, BUF_LEN);
    }
};

class ClientChannel : public ProactChannel
{
    char *_buf = NULL;
    int _sz = 0;
    int _count = 0;

public:
    ClientChannel()
    {
        _buf = (char*) ::malloc(BUF_LEN);
        _sz = 4;
    }

    ~ClientChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        ProactChannel::open(fd);
        NUT_LOG_D(TAG, "client channel opened");

        proactor.register_handler(this);
        proactor.launch_write(this, _buf, _sz);
    }

    virtual void handle_read_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "readed %d bytes from server", cb);
        assert(cb < BUF_LEN);
        _sz = cb;

        if (_count < 10 && _sz > 0)
            proactor.launch_write(this, _buf, _sz);
        else
            _sock_stream.close();
    }

    virtual void handle_write_completed(void *buf, int cb) override
    {
        NUT_LOG_D(TAG, "wrote %d bytes to server", cb);
        ++_count;

        proactor.launch_read(this, _buf, BUF_LEN);
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
        proactor.handle_events();
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
