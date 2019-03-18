
#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_reactor_manually"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345

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

class ServerChannel : public ReactChannel
{
    int _write_data = 0;

public:
    virtual void initialize() override
    {
        server = this;
        prepared = true;
    }

    virtual void handle_channel_connected() override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());
        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor.disable_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    }

    void write(int data)
    {
        _write_data = data;
        reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    void read()
    {
        reactor.enable_handler(this, ReactHandler::READ_MASK);
    }

    void close()
    {
        reactor.unregister_handler(this);
        _sock_stream.close();
        server = nullptr;
    }

    virtual void handle_read_ready() override
    {
        int data = 0;
        int rs = _sock_stream.read(&data, sizeof(data));
        NUT_LOG_D(TAG, "server received %d bytes: %d", rs, data);
        if (0 == rs) // 正常结束
        {
        //     NUT_LOG_D(TAG, "server will close");
        //     reactor.unregister_handler(this);
        //     _sock_stream.close();
        //     server = nullptr;
            // reactor.disable_handler(this, ReactHandler::READ_MASK);
            return;
        }
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_write_data, sizeof(_write_data));
        NUT_LOG_D(TAG, "server send %d bytes: %d", rs, _write_data);
        _write_data = 0;
        // assert(rs == sizeof(_write_data));

        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_io_exception(int err) override
    {
        NUT_LOG_D(TAG, "server exception %d", err);
    }
};

class ClientChannel : public ReactChannel
{
    int _write_data = 0;

public:
    virtual void initialize() override
    {
        client = this;
    }

    void write(int data)
    {
        _write_data = data;
        reactor.enable_handler(this, ReactHandler::WRITE_MASK);
    }

    void read()
    {
        reactor.enable_handler(this, ReactHandler::READ_MASK);
    }

    void close()
    {
        NUT_LOG_D(TAG, "client close");
        reactor.unregister_handler(this);
        _sock_stream.close();
        client = nullptr;
    }

    virtual void handle_channel_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());
        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor.disable_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
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

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_write_data, sizeof(_write_data));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _write_data);
        // assert(rs == sizeof(_write_data));
        _write_data = 0;

        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_io_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_reactor_manually()
{
    // start server
    InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
    rc_ptr<ReactAcceptor<ServerChannel> > acc = rc_new<ReactAcceptor<ServerChannel> >();
    acc->listen(addr);
    reactor.register_handler_later(acc, ReactHandler::ACCEPT_MASK);
    NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

    // start client
    NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
    ReactConnector<ClientChannel> con;
    con.connect(&reactor, addr);


    timewheel.add_timer(
        1000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // server->read();
            // SockOperation::shutdown_read(server->get_socket());
            // reactor.disable_handler(server, ReactHandler::READ_MASK);

            server->write(0);
        });

    timewheel.add_timer(
        2000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // server->read();
            client->close();
        });

    timewheel.add_timer(
        3000, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            int fd = server->get_socket();
            int e = SockOperation::get_last_error(fd);
            NUT_LOG_D(TAG, "-- %d, %d: %s",
                      SockOperation::is_valid(fd),
                      e, strerror(e));
            server->write(1);
        });

    // loop
    while (!prepared || server != nullptr || client != nullptr)
    {
        if (reactor.handle_events(2) < 0)
            break;
        timewheel.tick();
    }
}
