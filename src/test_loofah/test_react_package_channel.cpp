
#include <loofah/loofah.h>
#include <nut/nut.h>


#define TAG "test_react_package"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2348

using namespace nut;
using namespace loofah;

namespace
{

Reactor g_reactor;
TimeWheel g_timewheel;
class ServerChannel;
std::vector<rc_ptr<ServerChannel> > g_server_channels;

class ServerChannel : public ReactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_reactor(&g_reactor);
        set_time_wheel(&g_timewheel);

        // Hold reference
        g_server_channels.push_back(this);
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

    virtual void handle_reading_shutdown() override
    {
        NUT_LOG_D(TAG, "server reading shutdown, will close");
        close_later();
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "server closed");

        // Unhold reference
        for (size_t i = 0, sz = g_server_channels.size(); i < sz; ++i)
        {
            if (g_server_channels.at(i).pointer() == this)
            {
                g_server_channels.erase(g_server_channels.begin() + i);
                break;
            }
        }
    }
};

class ClientChannel : public ReactPackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_reactor(&g_reactor);
        set_time_wheel(&g_timewheel);
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

    virtual void handle_reading_shutdown() override
    {
        NUT_LOG_D(TAG, "client reading shutdown, will close");
        close_later();
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "client closed");
    }
};

}

void test_react_package_channel()
{
    // Start server
    rc_ptr<ReactAcceptor<ServerChannel>> acc = rc_new<ReactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc->open(addr);
    g_reactor.register_handler_later(acc, ReactHandler::READ_MASK);
    NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // Start client
    NUT_LOG_D(TAG, "client connect to %s", addr.to_string().c_str());
    rc_ptr<ClientChannel> client = ReactConnector<ClientChannel>::connect(addr);

    // Loop
    while (!g_server_channels.empty() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
    {
        if (g_reactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
            break;
        g_timewheel.tick();
    }
}
