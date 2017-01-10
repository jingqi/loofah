
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

void EventLoopBase::add_later_task(nut::Runnable *runnable)
{
    assert(NULL != runnable);
    nut::Guard<nut::Mutex> g(&_mutex);
    _later_tasks.push_back(runnable);
}

bool EventLoopBase::is_in_loop_thread() const
{
    return nut::Thread::tid_equals(_loop_tid, nut::Thread::current_thread_id());
}

void EventLoopBase::run_later_tasks()
{
    // NOTE This method can only be called from inside loop thread
    assert(is_in_loop_thread_and_not_handling());

    std::vector<nut::rc_ptr<nut::Runnable> > later_tasks;
    {
        nut::Guard<nut::Mutex> g(&_mutex);
        later_tasks = _later_tasks;
        _later_tasks.clear();
    }

    for (size_t i = 0, sz = later_tasks.size(); i < sz; ++i)
    {
        nut::Runnable *runnable = later_tasks.at(i);
        assert(NULL != runnable);
        runnable->run();
    }
}

void EventLoopBase::run_later(nut::Runnable *runnable)
{
    assert(NULL != runnable);

    if (is_in_loop_thread_and_not_handling())
    {
        runnable->run();
        return;
    }
    add_later_task(runnable);
}

}
