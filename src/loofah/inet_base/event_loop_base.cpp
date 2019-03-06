
#include "../loofah_config.h"

#include <nut/logging/logger.h>

#include "event_loop_base.h"


#define TAG "loofah.event_loop_base"

namespace loofah
{

EventLoopBase::EventLoopBase()
{
    _loop_tid = std::this_thread::get_id();
}

void EventLoopBase::add_later_task(task_type&& task)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _later_tasks.push_back(std::forward<task_type>(task));
}

void EventLoopBase::add_later_task(const task_type& task)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _later_tasks.push_back(task);
}

bool EventLoopBase::is_in_loop_thread() const
{
    return _loop_tid == std::this_thread::get_id();
}

bool EventLoopBase::is_in_loop_thread_and_not_handling() const
{
    return !_in_handling_events && is_in_loop_thread();
}

void EventLoopBase::run_later_tasks()
{
    // NOTE This method can only be called from inside loop thread
    assert(is_in_loop_thread_and_not_handling());

    std::vector<task_type> later_tasks;
    {
        std::lock_guard<std::mutex> guard(_mutex);
        later_tasks = _later_tasks;
        _later_tasks.clear();
    }

    for (size_t i = 0, sz = later_tasks.size(); i < sz; ++i)
        later_tasks.at(i)();
}

void EventLoopBase::run_later(task_type&& task)
{
    if (is_in_loop_thread_and_not_handling())
    {
        task();
        return;
    }
    add_later_task(std::forward<task_type>(task));
}

void EventLoopBase::run_later(const task_type& task)
{
    if (is_in_loop_thread_and_not_handling())
    {
        task();
        return;
    }
    add_later_task(task);
}

}
