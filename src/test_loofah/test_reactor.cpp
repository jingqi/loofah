
#include <loofah/reactor/reactor.h>
#include <loofah/reactor/react_acceptor.h>
#include <loofah/reactor/react_channel.h>
#include <loofah/reactor/react_connector.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif

#include <nut/logging/logger.h>
#include <nut/threading/thread_pool.h>


#define TAG "test_reactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345
#define BUF_LEN 256

using namespace nut;
using namespace loofah;

namespace
{

rc_ptr<ThreadPool> thread_pool;
Reactor reactor;

class ServerChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void open(socket_t fd) override
    {
        ReactChannel::open(fd);
        NUT_LOG_D(TAG, "server channel opened, fd %d", _sock_stream.get_socket());

        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "received %d bytes from client: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            _sock_stream.close();
            reactor.close();
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "send %d bytes to client: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }
};

class ClientChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void open(socket_t fd) override
    {
        ReactChannel::open(fd);
        NUT_LOG_D(TAG, "client channel opened, fd %d", _sock_stream.get_socket());

        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        //reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "received %d bytes from server: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            _sock_stream.close();
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        if (_counter > 20)
        {
            _sock_stream.close();
            return;
        }
        reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "send %d bytes to server: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }
};

void start_reactor_server(void*)
{
    ReactAcceptor<ServerChannel> acc;
    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc.open(addr);
    reactor.register_handler(&acc, ReactHandler::READ_MASK);
    NUT_LOG_D(TAG, "listening to %s, fd %d", addr.to_string().c_str(), acc.get_socket());
    while (true)
    {
        if (reactor.handle_events() < 0)
            break;
    }
    reactor.close();
    thread_pool->interrupt();
}

void start_reactor_client(void*)
{
#if NUT_PLATFORM_OS_WINDOWS
    ::Sleep(1000);
#else
    ::sleep(1); // Wait for server to be setupped
#endif

    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    ReactConnector con;
    ClientChannel *client = (ClientChannel*) ::malloc(sizeof(ClientChannel));
    new (client) ClientChannel;
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());
    con.connect(client, addr);
}

}

void test_reactor()
{
    thread_pool = rc_new<ThreadPool>(3);
    thread_pool->add_task(&start_reactor_server);
    thread_pool->add_task(&start_reactor_client);
    thread_pool->start();
    thread_pool->join();
}
