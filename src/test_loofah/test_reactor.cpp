
#include <loofah/reactor/reactor.h>
#include <loofah/reactor/sync_acceptor.h>
#include <loofah/reactor/sync_channel.h>
#include <loofah/reactor/sync_connector.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#endif

#include <nut/logging/logger.h>


#define TAG "test_reactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2345
#define BUF_LEN 256

using namespace loofah;


namespace
{

Reactor reactor;

class ServerChannel : public SyncChannel
{
    char *_buf = NULL;
    int _sz = 0;

public:
    ServerChannel()
    {
        _buf = (char*) ::malloc(BUF_LEN);
    }

    ~ServerChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        SyncChannel::open(fd);
        NUT_LOG_D(TAG, "server channel opened");

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = _sock_stream.read(_buf, BUF_LEN - 1);
        NUT_LOG_D(TAG, "readed %d bytes from client", _sz);
        if (0 == _sz) // 正常结束
            _sock_stream.close();
        else if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        _sock_stream.write(_buf, _sz);
        NUT_LOG_D(TAG, "wrote %d bytes to client", _sz);
        _sz = 0;
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

class ClientChannel : public SyncChannel
{
    char *_buf = NULL;
    int _sz = 0;
    int _count = 0;

public:
    ClientChannel()
    {
        _buf = (char*) ::malloc(BUF_LEN);
        _sz = 3;
    }

    ~ClientChannel()
    {
        ::free(_buf);
    }

    virtual void open(socket_t fd) override
    {
        SyncChannel::open(fd);
        NUT_LOG_D(TAG, "client channel opened");

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        //reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = _sock_stream.read(_buf, BUF_LEN - 1);
        NUT_LOG_D(TAG, "readed %d bytes from server", _sz);
        if (_count >= 10)
        {
            _sock_stream.close();
            return;
        }

        if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        _sock_stream.write(_buf, _sz);
        NUT_LOG_D(TAG, "wrote %d bytes to server", _sz);
        _sz = 0;
        ++_count;
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

}

void start_reactor_server(void*)
{
    SyncAcceptor<ServerChannel> acc;
    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc.open(addr);
    reactor.register_handler(&acc, SyncEventHandler::READ_MASK);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());
    while (true)
        reactor.handle_events();
}

void start_reactor_client(void*)
{
#if NUT_PLATFORM_OS_WINDOWS
    ::Sleep(1000);
#else
    ::sleep(1); // Wait for server to be setupped
#endif

    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    SyncConnector con;
    ClientChannel *client = (ClientChannel*) ::malloc(sizeof(ClientChannel));
    new (client) ClientChannel;
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());
    con.connect(client, addr);
}
