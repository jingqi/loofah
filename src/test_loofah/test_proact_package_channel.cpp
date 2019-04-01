﻿
#include <loofah/loofah.h>
#include <nut/nut.h>


#define TAG "test_proact_package"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2348

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Proactor proactor;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
rc_ptr<ClientChannel> client;
bool prepared = false;

class ServerChannel : public ProactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_proactor(&proactor);
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
        if (nullptr == pkg)
        {
            NUT_LOG_D(TAG, "server reading shutdown, will close");
            close_later();
            return;
        }

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

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "server closed");

        // Unhold reference
        server = nullptr;
    }
};

class ClientChannel : public ProactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_proactor(&proactor);
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
        if (nullptr == pkg)
        {
            NUT_LOG_D(TAG, "client reading shutdown, will close");
            close_later();
            return;
        }

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

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "client closed");
        client = nullptr;
    }
};

}

class TestProactPackageChannel : public TestFixture
{
    virtual void register_cases() override
    {
        NUT_REGISTER_CASE(test_proact_package_channel);
    }

    void test_proact_package_channel()
    {
        // Start server
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        rc_ptr<ProactAcceptor<ServerChannel>> acc = rc_new<ProactAcceptor<ServerChannel>>();
        acc->listen(addr);
        proactor.register_handler_later(acc);
        proactor.launch_accept_later(acc);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // Start client
        NUT_LOG_D(TAG, "client connect to %s", addr.to_string().c_str());
        ProactConnector<ClientChannel> con;
        con.connect(&proactor, addr);

        // Loop
        while (!prepared || server != nullptr || client != nullptr)
        {
            const uint64_t idle_ms = std::min<uint64_t>(
                60 * 1000, std::max<uint64_t>(TimeWheel::RESOLUTION_MS, timewheel.get_idle()));
            if (proactor.handle_events(idle_ms) < 0)
                break;
            timewheel.tick();
        }
    }
};

NUT_REGISTER_FIXTURE(TestProactPackageChannel, "proact, package, all")
