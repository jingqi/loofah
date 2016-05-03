
#include <assert.h>
#include <string.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#else
#   include <sys/socket.h> // for socket() and so on
#   include <netinet/in.h> // for struct sockaddr_in
#   include <unistd.h> // for close()
#endif

#include <errno.h>

#include <nut/logging/logger.h>

#include "sync_connector.h"
#include "../utils.h"

#define TAG "loofah.sync_connector"

namespace loofah
{

bool SyncConnector::connect(SyncStream *stream, const INETAddr& address)
{
    assert(NULL != stream);

    // new socket
    int fd = ::socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == fd)
    {
        NUT_LOG_E(TAG, "failed to call ::socket()");
        return false;
    }

    // connect
    const struct sockaddr_in& server_addr = address.get_sockaddr_in();
    const int status = ::connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (-1 == status)
    {
        NUT_LOG_E(TAG, "failed to call ::connect(), socketfd %d, errno %d", fd, errno);
        int save_errno = errno;
        ::close(fd);
        errno = save_errno; // the close() may be error
        return false;
    }

    // make nonblocking
    if (!make_socket_nonblocking(fd))
		NUT_LOG_W(TAG, "failed to make socket nonblocking, socketfd %d", fd);

    stream->open(fd);

    return true;
}

}
