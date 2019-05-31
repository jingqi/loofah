
#include "../loofah_config.h"

#include <nut/logging/logger.h>

#include "poller_base.h"


#define TAG "loofah.poller_base"

namespace loofah
{

PollerBase::PollerBase()
{
    _io_tid = std::this_thread::get_id();
}

void PollerBase::add_later_task(task_type&& task)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _later_tasks.push_back(std::forward<task_type>(task));
}

void PollerBase::add_later_task(const task_type& task)
{
    std::lock_guard<std::mutex> guard(_mutex);
    _later_tasks.push_back(task);
}

bool PollerBase::is_in_io_thread() const
{
    return _io_tid == std::this_thread::get_id();
}

bool PollerBase::is_in_io_thread_and_not_polling() const
{
    return !_in_polling && is_in_io_thread();
}

void PollerBase::run_later_tasks()
{
    // NOTE This method can only be called from inside IO thread
    assert(is_in_io_thread_and_not_polling());

    std::vector<task_type> later_tasks;
    {
        std::lock_guard<std::mutex> guard(_mutex);
        later_tasks = _later_tasks;
        _later_tasks.clear();
    }

    for (size_t i = 0, sz = later_tasks.size(); i < sz; ++i)
        later_tasks.at(i)();
}

void PollerBase::run_later(task_type&& task)
{
    if (is_in_io_thread_and_not_polling())
    {
        task();
        return;
    }
    add_later_task(std::forward<task_type>(task));
}

void PollerBase::run_later(const task_type& task)
{
    if (is_in_io_thread_and_not_polling())
    {
        task();
        return;
    }
    add_later_task(task);
}

}
