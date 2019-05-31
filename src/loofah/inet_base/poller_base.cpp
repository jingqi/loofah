
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
    _later_tasks.eliminate_enqueue(std::forward<task_type>(task));
}

void PollerBase::add_later_task(const task_type& task)
{
    _later_tasks.eliminate_enqueue(task);
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

    task_type task;
    while (_later_tasks.eliminate_dequeue(&task))
        task();
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
