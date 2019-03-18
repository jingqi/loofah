﻿/**
 * 测试 ProactPackageChannel 对 RST 状态的处理
 */

#include <loofah/loofah.h>
#include <nut/nut.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif


#define TAG "test_proact_package_rst"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2350

using namespace nut;
using namespace loofah;

namespace
{

class ServerChannel;
class ClientChannel;

Proactor proactor;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
ClientChannel *client = nullptr;
int prepared = 0;

void testing();

class ServerChannel : public ProactPackageChannel
{
public:
    virtual void initialize() override
    {
        set_proactor(&proactor);
        set_time_wheel(&timewheel);

        server = this;
    }

    void writeint(uint32_t data)
    {
        rc_ptr<Package> pkg = rc_new<Package>();
        pkg->set_little_endian(true);
        *pkg << data;
        write_later(pkg);
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());

        ++prepared;
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

class ClientChannel : public ProactChannel
{
    uint64_t _read_data = 0;

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
    }

    virtual void handle_connected() override
    {
        NUT_LOG_D(TAG, "client make a connection, fd %d", get_socket());

        proactor.register_handler(this);

        ++prepared;
        if (2 == prepared)
            testing();
    }

    virtual void handle_read_completed(size_t cb) override
    {
        uint32_t data = _read_data >> 32;
        NUT_LOG_D(TAG, "client received %d bytes: %d", cb, data);
    }

    virtual void handle_write_completed(size_t cb) override
    {
        NUT_LOG_D(TAG, "client send %d bytes", cb);
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
            SockOperation::shutdown_read(server->get_socket());

            // 1. client 不读数据
            // 2. 向 client 写数据
            // 3. 由于存在未读数据，client.close() 将发送 RST 包给 server
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
            NUT_LOG_D(TAG, "--- good");
            server->close();
        });
}

}

class TestProactPackageRST : public TestFixture
{
    virtual void register_cases() override
    {
        NUT_REGISTER_CASE(test_proact_pkg_rst);
    }

    void test_proact_pkg_rst()
    {
        // start server
        rc_ptr<ProactAcceptor<ServerChannel>> acc = rc_new<ProactAcceptor<ServerChannel>>();
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        acc->open(addr);
        proactor.register_handler_later(acc);
        proactor.launch_accept_later(acc);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // start client
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        nut::rc_ptr<ClientChannel> client = ProactConnector<ClientChannel>::connect(addr);

        // loop
        while (server != nullptr || LOOFAH_INVALID_SOCKET_FD != client->get_socket())
        {
            if (proactor.handle_events(TimeWheel::TICK_GRANULARITY_MS) < 0)
                break;
            timewheel.tick();
        }
    }
};

NUT_REGISTER_FIXTURE(TestProactPackageRST, "proact, package, RST, all")