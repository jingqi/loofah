
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
        NUT_LOG_D(TAG, "server got a connection, fd %d", _sock_stream.get_socket());
        g_reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "server received %d bytes: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            NUT_LOG_D(TAG, "server will close");
            g_reactor.unregister_handler(this);
            _sock_stream.close();
            for (size_t i = 0, sz = g_server_channels.size(); i < sz; ++i)
            {
                if (g_server_channels.at(i).pointer() == this)
                {
                    g_server_channels.erase(g_server_channels.begin() + i);
                    break;

                }
            }
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        g_reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "server send %d bytes: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
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
        NUT_LOG_D(TAG, "client make a connection, fd %d", _sock_stream.get_socket());
        g_reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        //g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "client received %d bytes: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            NUT_LOG_D(TAG, "client will close");
            g_reactor.unregister_handler(this);
            _sock_stream.close();
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        if (_counter > 20)
        {
            NUT_LOG_D(TAG, "client will close");
            g_reactor.unregister_handler(this);
            _sock_stream.close();
            return;
        }
        g_reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }
};

}

void test_reactor()
{
    // start server
    rc_ptr<ReactAcceptor<ServerChannel> > acc = rc_new<ReactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc->open(addr);
    g_reactor.register_handler_later(acc, ReactHandler::READ_MASK);
    NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // start client
    NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
    nut::rc_ptr<ClientChannel> client = ReactConnector<ClientChannel>::connect(addr);

    // loop
    while (!g_server_channels.empty() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
    {
        if (g_reactor.handle_events() < 0)
            break;
    }
}
