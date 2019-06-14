
#include "../loofah_config.h"

#include <nut/logging/logger.h>

#include "poller_base.h"


#define TAG "loofah.poller_base"

namespace loofah
{

PollerBase::PollerBase() noexcept
{
    _poller_tid = std::this_thread::get_id();
}

void PollerBase::add_task(task_type&& task) noexcept
{
    _deferred_tasks.eliminate_enqueue(std::forward<task_type>(task));
}

void PollerBase::add_task(const task_type& task) noexcept
{
    _deferred_tasks.eliminate_enqueue(task);
}

bool PollerBase::is_in_poll_thread() const noexcept
{
    return _poller_tid == std::this_thread::get_id();
}

void PollerBase::run_deferred_tasks() noexcept
{
    // NOTE This method can only be called from inside IO thread
    assert(is_in_poll_thread());

    task_type task;
    while (_deferred_tasks.eliminate_dequeue(&task))
        task();
}

void PollerBase::run_in_poll_thread(task_type&& task) noexcept
{
    if (is_in_poll_thread())
    {
        task();
        return;
    }

    add_task(std::forward<task_type>(task));
    wakeup_poll_wait();
}

void PollerBase::run_in_poll_thread(const task_type& task) noexcept
{
    if (is_in_poll_thread())
    {
        task();
        return;
    }

    add_task(task);
    wakeup_poll_wait();
}

}
