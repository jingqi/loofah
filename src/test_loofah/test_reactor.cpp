
#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_reactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345
#define BUF_LEN 256

using namespace nut;
using namespace loofah;

namespace
{

Reactor g_reactor;
class ServerChannel;
std::vector<rc_ptr<ServerChannel> > g_server_channels;

class ServerChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        g_server_channels.push_back(this);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server channel connected, fd %d", _sock_stream.get_socket());
        g_reactor.async_register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        g_reactor.async_disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "received %d bytes from client: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            _sock_stream.shutdown();
            g_reactor.async_unregister_handler(this);
            g_reactor.async_shutdown();
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        g_reactor.async_enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "send %d bytes to client: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        g_reactor.async_disable_handler(this, ReactHandler::WRITE_MASK);
    }
};

class ClientChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {}

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client channel connected, fd %d", _sock_stream.get_socket());
        g_reactor.async_register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        //g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "received %d bytes from server: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            g_reactor.async_unregister_handler(this);
            _sock_stream.shutdown();
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        if (_counter > 20)
        {
            g_reactor.async_unregister_handler(this);
            _sock_stream.shutdown();
            return;
        }
        g_reactor.async_enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "send %d bytes to server: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        g_reactor.async_disable_handler(this, ReactHandler::WRITE_MASK);
    }
};

}

void test_reactor()
{
    // start server
    rc_ptr<ReactAcceptor<ServerChannel> > acc = rc_new<ReactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc->open(addr);
    g_reactor.async_register_handler(acc, ReactHandler::READ_MASK);
    NUT_LOG_D(TAG, "listening to %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // start client
    nut::rc_ptr<ClientChannel> client = ReactConnector<ClientChannel>::connect(addr);
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());

    // loop
    while (true)
    {
        if (g_reactor.handle_events() < 0)
            break;
    }
    g_server_channels.clear();
    g_reactor.async_shutdown();
}
