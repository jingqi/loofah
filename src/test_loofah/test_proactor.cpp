
#include <loofah/proactor/proactor.h>
#include <loofah/proactor/async_acceptor.h>
#include <loofah/proactor/async_channel.h>
#include <loofah/proactor/async_connector.h>

#include <nut/platform/platform.h>

#include <nut/logging/logger.h>


#define TAG "test_proactor"
#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2346
#define BUF_LEN 256

using namespace loofah;

Proactor proactor;

class ServerStream : public AsyncChannel
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
};

void start_proactor_server(void*)
{}

void start_proactor_client(void*)
{}
