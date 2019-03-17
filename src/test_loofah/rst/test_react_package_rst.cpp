/**
 * 测试 ReactPackageChannel 对 RST 状态的处理
 */

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
        pkg->set_little_endian(true);
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

    void close()
    {
        g_reactor.unregister_handler(this);
        _sock_stream.close();
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());
        g_reactor.register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
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

        g_reactor.disable_handler(this, ReactHandler::WRITE_MASK);
    }

    virtual void handle_exception(int err) override
    {
        NUT_LOG_D(TAG, "client exception %d", err);
    }
};

}

void test_react_package_rst()
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
        50, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // 关闭 server 读通道，仅仅依靠写通道的自检来处理 RST 状态
            g_reactor.disable_handler(g_server, ReactHandler::READ_MASK);
            SockOperation::shutdown_read(g_server->get_socket());

            // 1. 关闭 client 读通道
            // 2. 向 client 写数据
            // 3. 由于存在未读数据，client.close() 将发送 RST 包给 server
            g_reactor.disable_handler(client, ReactHandler::READ_MASK);
            g_server->writeint(1);
            client->close();
        });

    g_timewheel.add_timer(
        100, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            // 如果没有正确处理 RST 状态，server 写数据会导致 SIGPIPE 信号从而
            // 进程退出
            g_server->writeint(2);
        });

    g_timewheel.add_timer(
        150, 0,
        [=](TimeWheel::timer_id_type id, uint64_t expires) {
            NUT_LOG_D(TAG, "----- good");
            g_server->close();
        });

    // loop
    while (!g_server.is_null() || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
    {
        if (g_reactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
            break;
        g_timewheel.tick();
    }
}
