
#include <loofah/reactor/reactor.h>
#include <loofah/reactor/sync_acceptor.h>
#include <loofah/reactor/sync_stream.h>
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

Reactor reactor;

class ServerStream : public SyncStream
{
    char *_buf = NULL;
    int _sz = 0;

public:
    ServerStream()
    {
        _buf = (char*) ::malloc(BUF_LEN);
    }

    ~ServerStream()
    {
        ::free(_buf);
    }

    virtual void open(loofah::socket_t fd) override
    {
        NUT_LOG_D(TAG, "server stream opened");
        SyncStream::open(fd);

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = ::read(_fd, _buf, BUF_LEN - 1);
        NUT_LOG_D(TAG, "readed %d bytes from client", _sz);
        if (0 == _sz) // 正常结束
            close();
        else if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        ::write(_fd, _buf, _sz);
        NUT_LOG_D(TAG, "wrote %d bytes to client", _sz);
        _sz = 0;
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

void start_reactor_server(void*)
{
    SyncAcceptor<ServerStream> acc;
    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    acc.open(addr);
    reactor.register_handler(&acc, SyncEventHandler::READ_MASK);
    NUT_LOG_D(TAG, "listening to %s", addr.to_string().c_str());
    while (true)
        reactor.handle_events();
}

class ClientStream : public SyncStream
{
    char *_buf = NULL;
    int _sz = 0;
    int _count = 0;

public:
    ClientStream()
    {
        _buf = (char*) ::malloc(BUF_LEN);
        _sz = 3;
    }

    ~ClientStream()
    {
        ::free(_buf);
    }

    void open(loofah::socket_t fd) override
    {
        NUT_LOG_D(TAG, "client stream opened");
        SyncStream::open(fd);

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        //reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = ::read(_fd, _buf, BUF_LEN - 1);
        NUT_LOG_D(TAG, "readed %d bytes from server", _sz);
        if (_count >= 10)
        {
            close();
            return;
        }

        if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        ::write(_fd, _buf, _sz);
        NUT_LOG_D(TAG, "wrote %d bytes to server", _sz);
        _sz = 0;
        ++_count;
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

void start_reactor_client(void*)
{
#if NUT_PLATFORM_OS_WINDOWS
    ::Sleep(1000);
#else
    ::sleep(1); // Wait for server to be setupped
#endif

    INETAddr addr(LISTEN_ADDR, LISTEN_PORT);
    SyncConnector con;
    ClientStream *client = (ClientStream*) ::malloc(sizeof(ClientStream));
    new (client) ClientStream;
    NUT_LOG_D(TAG, "will connect to %s", addr.to_string().c_str());
    con.connect(client, addr);
}
