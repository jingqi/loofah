
#include <assert.h>

#include "proact_handler.h"
#include "io_request.h"


namespace loofah
{

ProactHandler::~ProactHandler() noexcept
{
#if NUT_PLATFORM_OS_WINDOWS
    cancel_requests();
#else
    delete_requests();
#endif
}

#if NUT_PLATFORM_OS_WINDOWS
void ProactHandler::cancel_requests() noexcept
{
    for (IORequest* req : _read_queue)
    {
        assert(nullptr != req);
        req->canceled = true;
    }
    _read_queue.clear();
    for (IORequest* req : _write_queue)
    {
        assert(nullptr != req);
        req->canceled = true;
    }
    _write_queue.clear();
}
#else
void ProactHandler::delete_requests() noexcept
{
    while (!_read_queue.empty())
    {
        IORequest *io_request = _read_queue.front();
        assert(nullptr != io_request);
        _read_queue.pop_front();
        IORequest::delete_request(io_request);
    }

    while (!_write_queue.empty())
    {
        IORequest *io_request = _write_queue.front();
        assert(nullptr != io_request);
        _write_queue.pop_front();
        IORequest::delete_request(io_request);
    }
}
#endif

}
