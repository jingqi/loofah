
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
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());

        g_proactor.register_handler(this);

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.launch_read(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "server received %d bytes: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            NUT_LOG_D(TAG, "server will close");
            g_proactor.unregister_handler(this);
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

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "server send %d bytes: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.launch_read(this, &buf, &len, 1);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_E(TAG, "server exception %d", err);
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
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        g_proactor.register_handler(this);

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_read_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client received %d bytes: %d", cb, _tmp);
        if (0 == cb) // 正常结束
        {
            NUT_LOG_D(TAG, "client will close");
            g_proactor.unregister_handler(this);
            _sock_stream.close();
            return;
        }

        assert(cb == sizeof(_tmp));
        assert(_tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            NUT_LOG_D(TAG, "client will close");
            g_proactor.unregister_handler(this);
            _sock_stream.close();
            return;
        }

        void *buf = &_counter;
        size_t len = sizeof(_counter);
        g_proactor.launch_write(this, &buf, &len, 1);
    }

    virtual void handle_write_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client send %d bytes: %d", cb, _counter);
        assert(cb == sizeof(_counter));
        ++_counter;

        void *buf = &_tmp;
        size_t len = sizeof(_tmp);
        g_proactor.launch_read(this, &buf, &len, 1);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_E(TAG, "client exception %d", err);
    }
};

}

class TestProactor : public TestFixture
{
    virtual void register_cases() override
    {
        NUT_REGISTER_CASE(test_proactor);
    }

    void test_proactor()
    {
        // start server
        rc_ptr<ProactAcceptor<ServerChannel>> acc = rc_new<ProactAcceptor<ServerChannel>>();
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        acc->open(addr);
        g_proactor.register_handler_later(acc);
        g_proactor.launch_accept_later(acc);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // start client
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        rc_ptr<ClientChannel> client = ProactConnector<ClientChannel>::connect(addr);

        // loop
        while (!g_server_channels.empty() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
        {
            if (g_proactor.handle_events() < 0)
                break;
        }
    }
};

NUT_REGISTER_FIXTURE(TestProactor, "proact, all")
