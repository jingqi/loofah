
#include <loofah/loofah.h>
#include <nut/nut.h>


#define TAG "test_package"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2347

using namespace nut;
using namespace loofah;

namespace
{

Proactor g_proactor;
class ServerChannel;
std::vector<rc_ptr<ServerChannel> > g_server_channels;

class ServerChannel : public PackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_proactor(&g_proactor);

        // Hold reference
        g_server_channels.push_back(this);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server channel connected");
    }

    virtual void handle_read(Package *pkg) override
    {
        assert(NULL != pkg);
        NUT_LOG_D(TAG, "received %d bytes from client: %d", pkg->readable_size(), _counter);

        assert(pkg->readable_size() == sizeof(int));
        const int tmp = *(const int*) pkg->readable_data();
        assert(tmp == _counter);
        ++_counter;

        rc_ptr<Package> new_pkg = rc_new<Package>();
        new_pkg->write(&_counter, sizeof(_counter));
        async_write(new_pkg);
        ++_counter;
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

        g_proactor.async_shutdown();
    }
};

class ClientChannel : public PackageChannel
{
    int _counter = 0;

public:
    virtual void initialize() override
    {
        // Initialize
        set_proactor(&g_proactor);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client channel connected");

        rc_ptr<Package> new_pkg = rc_new<Package>();
        new_pkg->write(&_counter, sizeof(_counter));
        async_write(new_pkg);
        ++_counter;
    }

    virtual void handle_read(Package *pkg) override
    {
        assert(NULL != pkg);
        NUT_LOG_D(TAG, "received %d bytes from server: %d", pkg->readable_size(), _counter);

        assert(pkg->readable_size() == sizeof(int));
        const int tmp = *(const int*) pkg->readable_data();
        assert(tmp == _counter);
        ++_counter;

        if (_counter > 20)
        {
            async_close();
            return;
        }
        rc_ptr<Package> new_pkg = rc_new<Package>();
        new_pkg->write(&_counter, sizeof(_counter));
        async_write(new_pkg);
        ++_counter;
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "client closed");
    }
};

}

void test_package_channel()
{
    // start server
    rc_ptr<ProactAcceptor<ServerChannel> > acc = rc_new<ProactAcceptor<ServerChannel> >();
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc->open(addr);
    g_proactor.async_register_handler(acc);
    g_proactor.async_launch_accept(acc);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());

    // start client
    rc_ptr<ClientChannel> client = ProactConnector<ClientChannel>::connect(addr);
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());

    // loop
    while (true)
    {
        if (g_proactor.handle_events() < 0)
            break;
    }
    g_server_channels.clear();
    g_proactor.async_shutdown();
}
