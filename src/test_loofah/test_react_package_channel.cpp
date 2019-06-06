
#include <loofah/loofah.h>
#include <nut/nut.h>


#define TAG "test_react_package"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2347

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Reactor reactor;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
rc_ptr<ClientChannel> client;
bool prepared = false;

class ServerChannel : public ReactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_reactor(&reactor);
        set_time_wheel(&timewheel);

        // Hold reference
        server = this;
		prepared = true;
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());
    }

    virtual void handle_read(Package *pkg) override
    {
        assert(nullptr != pkg);
        NUT_LOG_D(TAG, "server received %d bytes: %d", pkg->readable_size(), _counter);

        assert(pkg->readable_size() == sizeof(int));
        int tmp = 0;
        *pkg >> tmp;
        assert(tmp == _counter);
        ++_counter;

        rc_ptr<Package> new_pkg = rc_new<Package>();
        *new_pkg << _counter;
        write(new_pkg);
        NUT_LOG_D(TAG, "server send %d", _counter);
        ++_counter;
    }

    virtual void handle_closed(int err) override
    {
        NUT_LOG_D(TAG, "server closed, %d: %s", err, str_error(err));

        // Unhold reference
		server = nullptr;
    }
};

class ClientChannel : public ReactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_reactor(&reactor);
        set_time_wheel(&timewheel);

		client = this;
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client create a connection, fd %d", get_socket());

        rc_ptr<Package> new_pkg = rc_new<Package>();
        *new_pkg << _counter;
        write(new_pkg);
        NUT_LOG_D(TAG, "client send %d", _counter);
        ++_counter;
    }

    virtual void handle_read(Package *pkg) override
    {
        assert(nullptr != pkg);
        NUT_LOG_D(TAG, "client received %d bytes: %d", pkg->readable_size(), _counter);

        assert(pkg->readable_size() == sizeof(int));
        int tmp = 0;
        *pkg >> tmp;
        assert(tmp == _counter);
        ++_counter;

        rc_ptr<Package> new_pkg = rc_new<Package>();
        *new_pkg << _counter;
        write(new_pkg);
        NUT_LOG_D(TAG, "client send %d", _counter);
        ++_counter;

        if (_counter > 20)
        {
            NUT_LOG_D(TAG, "client going to close");
            close_later();
            return;
        }
    }

    virtual void handle_closed(int err) override
    {
        NUT_LOG_D(TAG, "client closed, %d: %s", err, str_error(err));
		client = nullptr;
    }
};

}

class TestReactPackageChannel : public TestFixture
{
    virtual void register_cases() override
    {
        NUT_REGISTER_CASE(test_react_package_channel);
    }

    void test_react_package_channel()
    {
        // Start server
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        rc_ptr<ReactAcceptor<ServerChannel>> acc = rc_new<ReactAcceptor<ServerChannel> >();
        acc->listen(addr);
        reactor.register_handler_later(acc, ReactHandler::ACCEPT_MASK);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // Start client
        NUT_LOG_D(TAG, "client connect to %s", addr.to_string().c_str());
        ReactConnector<ClientChannel> con;
		con.connect(&reactor, addr);

        // Loop
        while (!prepared || server != nullptr || client != nullptr)
        {
            if (reactor.poll(timewheel.get_idle()) < 0)
                break;
            timewheel.tick();
        }
    }
};

NUT_REGISTER_FIXTURE(TestReactPackageChannel, "react, package, all")
