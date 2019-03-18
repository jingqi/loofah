/**
 * 测试 ReactPackageChannel 对 RST 状态的处理
 */

#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_react_package_rst"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2349

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Reactor reactor;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
ClientChannel *client = nullptr;
int prepared = 0;

void testing();

class ServerChannel : public ReactPackageChannel
{
public:
    virtual void initialize() override
    {
        set_reactor(&reactor);
        set_time_wheel(&timewheel);

        server = this;
    }

    void writeint(int data)
    {
        rc_ptr<Package> pkg = rc_new<Package>();
        pkg->set_little_endian(true);
        *pkg << data;
        write_later(pkg);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());

        prepared += 1;
        if (2 == prepared)
            testing();
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
        NUT_LOG_D(TAG, "server error %d: %s", err, str_error(err));
    }

    virtual void handle_close() override
    {
        NUT_LOG_D(TAG, "server closed");

        server = nullptr;
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

    void close()
    {
        NUT_LOG_D(TAG, "client close");
        reactor.unregister_handler(this);
        _sock_stream.close();
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());
        reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        reactor.disable_handler(this, ReactHandler::WRITE_MASK);

        prepared += 1;
        if (2 == prepared)
            testing();
    }

    virtual void handle_read_ready() override
    {
        uint32_t header = 0;
        int data = 0;
        int rs = _sock_stream.read(&header, sizeof(header));
        rs += _sock_stream.read(&data, sizeof(data));
        NUT_LOG_D(TAG, "client received %d bytes: %d", rs, data);
    }

    virtual void handle_write_ready() override
    {
        int rs = _sock_stream.write(&_write_data, sizeof(_write_data));
        NUT_LOG_D(TAG, "client send %d bytes: %d", rs, _write_data);
        _write_data = 0;

        reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d: %s", err, str_error(err));
    }
};

void testing()
{
    timewheel.add_timer(
        50, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // 关闭 server 读通道，仅仅依靠写通道的自检来处理 RST 状态
            reactor.disable_handler(server, ReactHandler::READ_MASK);
            SockOperation::shutdown_read(server->get_socket());

            // 1. 关闭 client 读通道
            // 2. 向 client 写数据
            // 3. 由于存在未读数据，client.close() 将发送 RST 包给 server
            reactor.disable_handler(client, ReactHandler::READ_MASK);
            server->writeint(1);
            client->close();
        });

    timewheel.add_timer(
        100, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // 如果没有正确处理 RST 状态，server 写数据会导致 SIGPIPE 信号从而
            // 进程退出
            server->writeint(2);
        });

    timewheel.add_timer(
        150, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            NUT_LOG_D(TAG, "----- good");
            server->close();
        });
}

}

class TestReactPackageRST : public TestFixture
{
    virtual void register_cases() override
    {
        NUT_REGISTER_CASE(test_react_pkg_rst);
    }

    void test_react_pkg_rst()
    {
        // start server
        rc_ptr<ReactAcceptor<ServerChannel> > acc = rc_new<ReactAcceptor<ServerChannel> >();
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        acc->open(addr);
        reactor.register_handler_later(acc, ReactHandler::READ_MASK);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // start client
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        nut::rc_ptr<ClientChannel> refclient = ReactConnector<ClientChannel>::connect(addr);

        // loop
        while (server != nullptr || LOOFAH_INVALID_SOCKET_FD != refclient->get_socket())
        {
            if (reactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
                break;
            timewheel.tick();
        }
    }

};

NUT_REGISTER_FIXTURE(TestReactPackageRST, "react, package, RST, all")
