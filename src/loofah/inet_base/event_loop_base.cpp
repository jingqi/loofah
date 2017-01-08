
#include <stdlib.h> // for NULL

#include <nut/threading/sync/guard.h>
#include <nut/logging/logger.h>

#include "event_loop_base.h"


#define TAG "loofah.event_loop_base"

namespace loofah
{

EventLoopBase::EventLoopBase()
{
    _loop_tid = nut::Thread::current_thread_id();
}

void EventLoopBase::add_async_task(nut::Runnable *runnable)
{
    assert(NULL != runnable);
    nut::Guard<nut::Mutex> g(&_mutex);
    _async_tasks.push_back(runnable);
}

bool EventLoopBase::is_in_loop_thread() const
{
    return nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id());
}

void EventLoopBase::run_in_loop_thread(nut::Runnable *runnable)
{
    assert(NULL != runnable);

    if (nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id()))
    {
        runnable->run();
        return;
    }
    add_async_task(runnable);
}

void EventLoopBase::run_async_tasks()
{
    // NOTE This method can only be called from inside loop thread
    assert(is_in_loop_thread());

    std::vector<nut::rc_ptr<nut::Runnable> > async_tasks;
    {
        nut::Guard<nut::Mutex> g(&_mutex);
        async_tasks = _async_tasks;
        _async_tasks.clear();
    }

    for (size_t i = 0, sz = async_tasks.size(); i < sz; ++i)
    {
        nut::Runnable *runnable = async_tasks.at(i);
        assert(NULL != runnable);
        runnable->run();
    }
}

}
