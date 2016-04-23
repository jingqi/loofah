
#include "sync_stream.h"

namespace loofah
{

void SyncStream::open(handle_t fd)
{
    _fd = fd;
}

}
