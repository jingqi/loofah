
#ifndef ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_
#define ___HEADFILE_64C1BFFB_AE33_4DBD_AAE2_542809F526DE_

#include "../loofah_config.h"

#include <queue>

#include <nut/platform/platform.h>
#include <nut/threading/sync/mutex.h>

namespace loofah
{

class ProactHandler
{
#if NUT_PLATFORM_OS_MAC || NUT_PLATFORM_OS_LINUX
    int _registered_events = 0;
    int _request_accept = 0;
    std::queue<void*> _read_queue, _write_queue;
    nut::Mutex _mutex;

    friend class Proactor;
#endif

public:
    enum EventType
    {
        ACCEPT_MASK = 1,
        READ_MASK = 1 << 1,
        WRITE_MASK = 1 << 2,
        EXCEPT_MASK = 1 << 3,
    };

    virtual ~ProactHandler() {}

    virtual socket_t get_socket() const = 0;

    virtual void handle_accept_completed(socket_t fd) {}
    virtual void handle_read_completed(int cb) {}
    virtual void handle_write_completed(int cb) {}
};

}

#endif
