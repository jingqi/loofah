
#include <assert.h>

#include "proact_handler.h"
#include "io_request.h"


namespace loofah
{

ProactHandler::~ProactHandler()
{
    delete_requests();
}

void ProactHandler::delete_requests()
{
    while (!_read_queue.empty())
    {
        IORequest *io_request = _read_queue.front();
        assert(nullptr != io_request);
        _read_queue.pop();
        IORequest::delete_request(io_request);
    }

    while (!_write_queue.empty())
    {
        IORequest *io_request = _write_queue.front();
        assert(nullptr != io_request);
        _write_queue.pop();
        IORequest::delete_request(io_request);
    }
}

}
