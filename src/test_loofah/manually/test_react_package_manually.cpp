
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

class ServerChannel;
class ClientChannel;

Reactor reactor;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
rc_ptr<ClientChannel> client;
bool prepared = false;

class ServerChannel : public ReactPackageChannel
{
public:
    virtual void initialize() noexcept override
    {
        set_reactor(&reactor);
        set_time_wheel(&timewheel);

        server = this;
		prepared = true;
    }

    void writeint(int data) noexcept
    {
        rc_ptr<Package> pkg = rc_new<Package>();
        *pkg << data;
        write(pkg);
    }

    virtual void handle_connected() noexcept override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());
    }

    virtual void handle_read(Package *pkg) noexcept override
    {
        assert(nullptr != pkg);
        int rs = pkg->readable_size();
        int data = 0;
        *pkg >> data;
        NUT_LOG_D(TAG, "server received %d bytes: %d", rs, data);
    }

    virtual void handle_closed(int err) noexcept override
    {
        NUT_LOG_D(TAG, "server closed, %d: %s", err, str_error(err));

        server = nullptr;
    }
};

class ClientChannel : public ReactChannel
{
    int _write_data = 0;

public:
    virtual void initialize() noexcept override
    {
		client = this;
	}

    void write(int data) noexcept
    {
        _write_data = data;
        reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    void read() noexcept
    {
        reactor.enable_handler(this, ReactHandler::READ_MASK);
    }

    void close() noexcept
    {
        NUT_LOG_D(TAG, "client close");
        reactor.unregister_handler(this);
        _sock_stream.close();
		client = nullptr;
    }

    virtual void handle_channel_connected() noexcept override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor.disable_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() noexcept override
    {
        int data = 0;
        int rs = _sock_stream.read(&data, sizeof(data));
        NUT_LOG_D(TAG, "client received %d bytes: %d", rs, data, errno);
        if (0 == rs) // 正常结束
        {
        //     NUT_LOG_D(TAG, "client will close");
        //     reactor.unregister_handler(this);
        //     _sock_stream.close();
            reactor.disable_handler(this, ReactHandler::READ_MASK);
            return;
        }
    }

    virtual void handle_write_ready() noexcept override
    {
        int rs = _sock_stream.write(&_write_data, sizeof(_write_data));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _write_data);
        // assert(rs == sizeof(_write_data));
        _write_data = 0;

        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_io_error(int err) noexcept override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_react_package_manually() noexcept
{
    reactor.initialize();

    // start server
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    rc_ptr<ReactAcceptor<ServerChannel>> acc = rc_new<ReactAcceptor<ServerChannel>>();
    acc->listen(addr);
    reactor.register_handler(acc, ReactHandler::ACCEPT_MASK);
    NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // start client
    NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
    ReactConnector<ClientChannel> con;
	con.connect(&reactor, addr);

    timewheel.add_timer(
        500, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // server->read();
            SockOperation::shutdown_read(server->get_socket());
            reactor.disable_handler(server, ReactHandler::READ_MASK);

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
            // int fd = server->get_socket();
            // int e = SockOperation::get_last_error(fd);
            // int e2 = SockOperation::get_last_error(fd);
            // NUT_LOG_D(TAG, "-- %d, %d(%d): %s",
            //           SockOperation::is_valid(fd),
            //           e, e2, strerror(e));

            server->writeint(1);
        });

    // loop
    while (!prepared || server != nullptr || client != nullptr)
    {
        if (reactor.poll(timewheel.get_idle()) < 0)
            break;
        timewheel.tick();
    }

    reactor.shutdown();
}
