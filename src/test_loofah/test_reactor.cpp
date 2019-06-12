
#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_reactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Reactor *reactor = nullptr;
rc_ptr<ServerChannel> server;
rc_ptr<ClientChannel> client;
bool prepared = false;

class ServerChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void initialize() noexcept override
    {
        server = this;
		prepared = true;
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());
        reactor->register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor->disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() noexcept override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "server received %d bytes: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            NUT_LOG_D(TAG, "server will close");
            reactor->unregister_handler(this);
            _sock_stream.close();
            server = nullptr;
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        reactor->enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() noexcept override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "server send %d bytes: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        reactor->disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_io_error(int err) noexcept override
    {
        NUT_LOG_E(TAG, "server exception %d", err);
    }
};

class ClientChannel : public ReactChannel
{
    int _counter = 0;

public:
    virtual void initialize() noexcept override
    {
        client = this;
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());
        reactor->register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        //reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() noexcept override
    {
        int seq = 0;
        int rs = _sock_stream.read(&seq, sizeof(seq));
        NUT_LOG_D(TAG, "client received %d bytes: %d", rs, seq);
        if (0 == rs) // 正常结束
        {
            NUT_LOG_D(TAG, "client will close");
            reactor->unregister_handler(this);
            _sock_stream.close();
            client = nullptr;
            return;
        }

        assert(rs == sizeof(seq));
        assert(seq == _counter);
        ++_counter;

        if (_counter > 20)
        {
            NUT_LOG_D(TAG, "client will close");
            reactor->unregister_handler(this);
            _sock_stream.close();
            client = nullptr;
            return;
        }
        reactor->enable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() noexcept override
    {
        int rs = _sock_stream.write(&_counter, sizeof(_counter));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _counter);
        assert(rs == sizeof(_counter));
        ++_counter;
        reactor->disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_io_error(int err) noexcept override
    {
        NUT_LOG_E(TAG, "client exception %d", err);
    }
};

}

class TestReactor : public TestFixture
{
    virtual void register_cases() noexcept override
    {
        NUT_REGISTER_CASE(test_reactor);
    }

    virtual void set_up() override
    {
        reactor = new Reactor;
    }

    virtual void tear_down() override
    {
        delete reactor;
        reactor = nullptr;
    }

    void test_reactor()
    {
        // start server
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        rc_ptr<ReactAcceptor<ServerChannel>> acc = rc_new<ReactAcceptor<ServerChannel>>();
        acc->listen(addr);
        reactor->register_handler_later(acc, ReactHandler::ACCEPT_MASK);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // start client
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        ReactConnector<ClientChannel> con;
        con.connect(reactor, addr);

        // loop
        while (!prepared || server != nullptr || client != nullptr)
        {
            if (reactor->poll() < 0)
                break;
        }
    }

};

NUT_REGISTER_FIXTURE(TestReactor, "react, all")
