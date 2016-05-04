
#include "sync_stream.h"

namespace loofah
{

void SyncStream::open(socket_t fd)
{
    _fd = fd;
}

}
