
#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_MAC
#   include <sys/types.h>
#   include <sys/event.h>
#   include <sys/time.h>
#endif

#include <nut/logging/logger.h>

#include "proactor.h"

#define TAG "loofah.proactor"

namespace loofah
{

Proactor::Proactor()
{
#if NUT_PLATFORM_OS_MAC
    _kq = ::kqueue();
#endif
}

}
