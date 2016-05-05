
#include <assert.h>
#include <new>

#include "async_stream.h"

// see http://blog.csdn.net/piggyxp/article/details/6922277

namespace loofah
{

void AsyncStream::open(socket_t fd)
{
    assert(INVALID_SOCKET_VALUE != fd);

    _fd = fd;
}

}
