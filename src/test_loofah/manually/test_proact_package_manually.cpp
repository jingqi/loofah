
#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_package_manually"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345
#define BUF_LEN 256

using namespace nut;
using namespace loofah;

namespace
{

Proactor g_proactor;
TimeWheel g_timewheel;

class ServerChannel;
rc_ptr<ServerChannel> g_server;

class ServerChannel : public ProactPackageChannel
{
public:
    virtual void initialize() override
    {
        set_proactor(&g_proactor);
        set_time_wheel(&g_timewheel);

        g_server = this;
    }

    void writeint(int data)
    {
        rc_ptr<Package> pkg = rc_new<Package>();
        *pkg << data;
        write_later(pkg);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());
    }

    virtual void handle_read(Package *pkg) override
    {
        if (nullptr == pkg)
        {
            NUT_LOG_D(TAG, "server reading shutdown");
            return;
        }

        int rs = pkg->readable_size();
        int data = 0;
        *pkg >> data;
        NUT_LOG_D(TAG, "server received %d bytes: %d", rs, data);
    }

    virtual void handle_error(int err) override
    {
        NUT_LOG_D(TAG, "server error %d", err);
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "server closed");

        g_server = nullptr;
    }
};

class ClientChannel : public ProactChannel
{
    int _read_data = 0;

public:
    virtual void initialize() override
    {}

    void write(int data)
    {
        void *buf = &data;
        size_t len = sizeof(data);
        g_proactor.launch_write(this, &buf, &len, 1);
    }

    void read()
    {
        void *buf = &_read_data;
        size_t len = sizeof(_read_data);
        g_proactor.launch_read(this, &buf, &len, 1);
    }

    void close()
    {
        NUT_LOG_D(TAG, "client close");
        g_proactor.unregister_handler(this);
        _sock_stream.close();
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        g_proactor.register_handler(this);
    }

    virtual void handle_read_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client received %d bytes: %d", cb, _read_data);
    }

    virtual void handle_write_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client send %d bytes", cb);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_proact_package_manually()
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
    nut::rc_ptr<ClientChannel> client = ProactConnector<ClientChannel>::connect(addr);

    g_timewheel.add_timer(
        500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // g_server->read();
            SockOperation::shutdown_read(g_server->get_socket());

            g_server->writeint(0);
        });

    g_timewheel.add_timer(
        1000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // g_server->read();
            client->close();
        });

    g_timewheel.add_timer(
        1500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            int fd = g_server->get_socket();
            // int e = SockOperation::get_last_error(fd);
            // NUT_LOG_D(TAG, "current --> %d, %d: %s",
            //           SockOperation::is_valid(fd),
            //           e, strerror(e));

            g_server->writeint(1);
        });

    g_timewheel.add_timer(
        2000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            NUT_LOG_D(TAG, "good");
        });


    // loop
    while (!g_server.is_null() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
    {
        if (g_proactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
            break;
        g_timewheel.tick();
    }
}
