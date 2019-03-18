
#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_package_manually"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345

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
public:
    virtual void initialize() override
    {
        set_proactor(&proactor);
        set_time_wheel(&timewheel);

        server = this;
        prepared = true;
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

    virtual void handle_exception(int err) override
    {
        NUT_LOG_D(TAG, "server error %d", err);
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "server closed");

        server = nullptr;
    }
};

class ClientChannel : public ProactChannel
{
    int _read_data = 0;

public:
    virtual void initialize() override
    {
        client = this;
    }

    void write(int data)
    {
        void *buf = &data;
        size_t len = sizeof(data);
        proactor.launch_write(this, &buf, &len, 1);
    }

    void read()
    {
        void *buf = &_read_data;
        size_t len = sizeof(_read_data);
        proactor.launch_read(this, &buf, &len, 1);
    }

    void close()
    {
        NUT_LOG_D(TAG, "client close");
        proactor.unregister_handler(this);
        _sock_stream.close();
        client = nullptr;
    }

    virtual void handle_channel_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        proactor.register_handler(this);
    }

    virtual void handle_read_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client received %d bytes: %d", cb, _read_data);
    }

    virtual void handle_write_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client send %d bytes", cb);
    }

    virtual void handle_io_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_proact_package_manually()
{
    // start server
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    rc_ptr<ProactAcceptor<ServerChannel>> acc = rc_new<ProactAcceptor<ServerChannel>>();
    acc->listen(addr);
    proactor.register_handler_later(acc);
    proactor.launch_accept_later(acc);
    NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // start client
    NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
    ProactConnector<ClientChannel> con;
    con.connect(&proactor, addr);

    timewheel.add_timer(
        500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // server->read();
            SockOperation::shutdown_read(server->get_socket());

            server->writeint(0);
        });

    timewheel.add_timer(
        1000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // server->read();
            client->close();
        });

    timewheel.add_timer(
        1500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            int fd = server->get_socket();
            // int e = SockOperation::get_last_error(fd);
            // NUT_LOG_D(TAG, "current --> %d, %d: %s",
            //           SockOperation::is_valid(fd),
            //           e, strerror(e));

            server->writeint(1);
        });

    timewheel.add_timer(
        2000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            NUT_LOG_D(TAG, "good");
        });


    // loop
    while (!prepared || server != nullptr || client != nullptr)
    {
        if (proactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
            break;
        timewheel.tick();
    }
}
