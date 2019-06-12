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

Reactor *reactor = nullptr;
TimeWheel timewheel;

rc_ptr<ServerChannel> server;
bool prepared = false;

class ServerChannel : public ReactPackageChannel
{
public:
    virtual void initialize() noexcept override
    {
        set_reactor(reactor);
        set_time_wheel(&timewheel);

        server = this;
        prepared = true;
    }

    void writeint(int data)
    {
        rc_ptr<Package> pkg = rc_new<Package>();
        pkg->set_little_endian(true);
        *pkg << data;
        write_later(pkg);
    }

    virtual void handle_connected() noexcept override
    {
        NUT_LOG_D(TAG, "server got a connection, fd %d", get_socket());

        writeint(2);
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

}

class TestReactPackageRST : public TestFixture
{
    virtual void register_cases() noexcept override
    {
        NUT_REGISTER_CASE(test_react_pkg_rst);
    }

    virtual void set_up() override
    {
        reactor = new Reactor;
    }

    virtual void tear_down() override
    {
        delete reactor;
        reactor = nullptr;
    }

    void test_react_pkg_rst()
    {
        // Start server
        InetAddr addr(LISTEN_ADDR, LISTEN_PORT);
        rc_ptr<ReactAcceptor<ServerChannel>> acc = rc_new<ReactAcceptor<ServerChannel>>();
        acc->listen(addr);
        reactor->register_handler_later(acc, ReactHandler::ACCEPT_MASK);
        NUT_LOG_D(TAG, "server listening at %s, fd %d", addr.to_string().c_str(), acc->get_socket());

        // Client
        const socket_t client_fd = ::socket(PF_INET, SOCK_STREAM, 0);
        NUT_LOG_D(TAG, "client will connect to %s", addr.to_string().c_str());
        ::connect(client_fd, addr.cast_to_sockaddr(), addr.get_sockaddr_size());

        // Close client later, send RST
        timewheel.add_timer(
            50, 0,
            [=] (TimeWheel::timer_id_type id, uint64_t expires) {
                // 由于存在未读数据，close() 将发送 RST 包给 server
                SockOperation::close(client_fd);
            });

        // loop
        while (!prepared || server != nullptr)
        {
            if (reactor->poll(timewheel.get_idle()) < 0)
                break;
            timewheel.tick();
        }
    }

};

NUT_REGISTER_FIXTURE(TestReactPackageRST, "react, package, RST, all")
