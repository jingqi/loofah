
#include <loofah/loofah.h>
#include <nut/nut.h>

#if !NUT_PLATFORM_OS_WINDOWS
#   include <unistd.h>
#endif


#define TAG "test_proactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2346

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Proactor *proactor = nullptr;

rc_ptr<ServerChannel> server;
rc_ptr<ClientChannel> client;
bool prepared = false;

class ServerChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void initialize() noexcept override
    {
        server = this;
        prepared = true;
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());

        proactor->register_handler(this);

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor->launch_read(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(size_t cb) noexcept override
    {
        NUT_LOG_D(TAG, "server received %d bytes: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            NUT_LOG_D(TAG, "server will close");
            proactor->unregister_handler(this);
            _sock_stream.close();
            server = nullptr;
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor->launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(size_t cb) noexcept override
    {
        NUT_LOG_D(TAG, "server send %d bytes: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor->launch_read(this, &buf, &len, 1);
    }

    virtual void handle_io_error(int err) noexcept override
    {
        NUT_LOG_E(TAG, "server exception %d", err);
    }
};

class ClientChannel : public ProactChannel
{
    int _counter = 0;
    int _tmp = 0;

public:
    virtual void initialize() noexcept override
    {
        client = this;
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        proactor->register_handler(this);

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor->launch_write(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(size_t cb) noexcept override
    {
        NUT_LOG_D(TAG, "client received %d bytes: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            NUT_LOG_D(TAG, "client will close");
            proactor->unregister_handler(this);
            _sock_stream.close();
            client = nullptr;
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            NUT_LOG_D(TAG, "client will close");
            proactor->unregister_handler(this);
            _sock_stream.close();
            client = nullptr;
            return;
        }

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        proactor->launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(size_t cb) noexcept override
    {
        NUT_LOG_D(TAG, "client send %d bytes: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        proactor->launch_read(this, &buf, &len, 1);
    }

    virtual void handle_io_error(int err) noexcept override
    {
        NUT_LOG_E(TAG, "client exception %d", err);
    }
};

}

class TestProactor : public TestFixture
{
    virtual void register_cases() noexcept override
    {
        NUT_REGISTER_CASE(test_proactor);
    }

    virtual void set_up() override
    {
        proactor = new Proactor;
    }

    virtual void tear_down() override
    {
        delete proactor;
        proactor = nullptr;
    }

    void test_proactor()
    {
        // start server
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        rc_ptr<ProactAcceptor<ServerChannel>> acc = rc_new<ProactAcceptor<ServerChannel>>();
        acc->listen(addr);
        proactor->register_handler_later(acc);
        proactor->launch_accept_later(acc);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // start client
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        ProactConnector<ClientChannel> con;
        con.connect(proactor, addr);

        // loop
        while (!prepared || server != nullptr || client != nullptr)
        {
            if (proactor->poll() < 0)
                break;
        }
    }
};

NUT_REGISTER_FIXTURE(TestProactor, "proact, all")
