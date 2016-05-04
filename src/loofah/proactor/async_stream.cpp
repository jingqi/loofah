
#include "async_stream.h"

namespace loofah
{

void AsyncStream::open(socket_t fd, Proactor *proactor)
{
    assert(INVALID_SOCKET_VALUE != fd && NULL != proactor);

    _fd = fd;
    _proactor = proactor;
}

void AsyncStream::launch_read()
{
    // see http://blog.csdn.net/piggyxp/article/details/6922277
    struct IOContext io_context(this, AsyncEventHandler::READ_MASK, 0);
    DWORD bytes = 0;
    ::WSARecv(_fd, &io_context.wsabuf, 1, &bytes, 0, (LPWSAOVERLAPPED)&io_context, NULL);
}

void AsyncStream::launch_write()
{
    struct IOContext io_context(this, AsyncEventHandler::READ_MASK, 0);
    DWORD bytes = 0;
    ::WSASend(_fd, &io_context.wsabuf, 1, &bytes, 0, (LPWSAOVERLAPPED)&io_context, NULL);
}

}
