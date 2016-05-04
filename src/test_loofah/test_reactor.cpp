
#include <loofah/reactor/reactor.h>
#include <loofah/reactor/sync_acceptor.h>
#include <loofah/reactor/sync_stream.h>
#include <loofah/reactor/sync_connector.h>

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

    void open(loofah::handle_t fd) override
    {
        SyncStream::open(fd);

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = ::read(_fd, &_buf, BUF_LEN - 1);
        if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        ::write(_fd, _buf, _sz);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

void start_reactor_server(void*)
{
	SyncAcceptor<ServerStream> acc;
	acc.open(2345);
	reactor.register_handler(&acc, SyncEventHandler::READ_MASK);
    while (true)
        reactor.handle_events();
}

class ClientStream : public SyncStream
{
    char *_buf = NULL;
    int _sz = 0;

public:
    ClientStream()
    {
        _buf = (char*) ::malloc(BUF_LEN);
    }

    ~ClientStream()
    {
        ::free(_buf);
    }

    void open(loofah::handle_t fd) override
    {
        SyncStream::open(fd);

        reactor.register_handler(this, SyncEventHandler::READ_MASK | SyncEventHandler::WRITE_MASK);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_read_ready() override
    {
        _sz = ::read(_fd, &_buf, BUF_LEN - 1);
        if (_sz > 0)
            reactor.enable_handler(this, SyncEventHandler::WRITE_MASK);
    }

    virtual void handle_write_ready() override
    {
        ::write(_fd, _buf, _sz);
        reactor.disable_handler(this, SyncEventHandler::WRITE_MASK);
    }
};

void start_reactor_client(void*)
{
    ::sleep(3); // sleep 3 seconds

    INETAddr addr("localhost", 2345);
    SyncConnector con;
    ClientStream client;
    con.connect(&client, addr);
}
