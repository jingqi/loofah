
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

Reactor g_reactor;
TimeWheel g_timewheel;

class ServerChannel;
rc_ptr<ServerChannel> g_server;

class ServerChannel : public ReactPackageChannel
{
public:
    virtual void initialize() override
    {
        set_reactor(&g_reactor);
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

class ClientChannel : public ReactChannel
{
    int _write_data = 0;

public:
    virtual void initialize() override
    {}

    void write(int data)
    {
        _write_data = data;
        g_reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    void read()
    {
        g_reactor.enable_handler(this, ReactHandler::READ_MASK);
    }

    void close()
    {
        NUT_LOG_D(TAG, "client close");
        g_reactor.unregister_handler(this);
        _sock_stream.close();
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        g_reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        g_reactor.disable_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        int data = 0;
        int rs = _sock_stream.read(&data, sizeof(data));
        NUT_LOG_D(TAG, "client received %d bytes: %d", rs, data, errno);
        if (0 == rs) // 正常结束
        {
        //     NUT_LOG_D(TAG, "client will close");
        //     g_reactor.unregister_handler(this);
        //     _sock_stream.close();
            g_reactor.disable_handler(this, ReactHandler::READ_MASK);
            return;
        }
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_write_data, sizeof(_write_data));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _write_data);
        // assert(rs == sizeof(_write_data));
        _write_data = 0;

        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_react_package_manually()
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

    g_timewheel.add_timer(
        500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // g_server->read();
            SockOperation::shutdown_read(g_server->get_socket());
            g_reactor.disable_handler(g_server, ReactHandler::READ_MASK);

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
            // int fd = g_server->get_socket();
            // int e = SockOperation::get_last_error(fd);
            // int e2 = SockOperation::get_last_error(fd);
            // NUT_LOG_D(TAG, "-- %d, %d(%d): %s",
            //           SockOperation::is_valid(fd),
            //           e, e2, strerror(e));

            g_server->writeint(1);
        });

    // loop
    while (!g_server.is_null() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
    {
        if (g_reactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
            break;
        g_timewheel.tick();
    }
}
