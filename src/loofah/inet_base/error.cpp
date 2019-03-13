
#include "../loofah_config.h"

#include <nut/platform/platform.h>

#include "error.h"


namespace loofah
{

int from_errno(int err)
{
#if NUT_PLATFORM_OS_WINDOWS
    switch (err)
    {
    case WSAEWOULDBLOCK:
        return LOOFAH_ERR_WOULD_BLOCK;

    case WSA_INVALID_HANDLE:
        return LOOFAH_ERR_INVALID_FD;

    case WSAECONNRESET:
        return LOOFAH_ERR_CONNECTION_RESET;

    // case WSAESHUTDOWN:
    }
#else
    switch (err)
    {
    case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return LOOFAH_ERR_WOULD_BLOCK;

    case ECONNRESET:
        return LOOFAH_ERR_CONNECTION_RESET;
    }
#endif

    return LOOFAH_ERR_UNKNOWN;
}

const char* str_error(int err)
{
    switch (err)
    {
    case LOOFAH_ERR_UNKNOWN:
        return "Unknown error";

    case LOOFAH_ERR_WOULD_BLOCK:
        return "Operation would bolck";

    case LOOFAH_ERR_INVALID_FD:
#if NUT_PLATFORM_OS_WINDOWS
        return "Invalid handle";
#else
        return "Invalid file descriptor";
#endif

    case LOOFAH_ERR_CONNECTION_RESET:
        return "Connection reset";

    case LOOFAH_ERR_PKG_OVERSIZE:
        return "Package payload size is too big";
    }
    return "(noerr)";
}

}
