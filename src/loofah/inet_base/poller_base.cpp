
#include "../loofah_config.h"

#include <nut/logging/logger.h>

#include "poller_base.h"


#define TAG "loofah.poller_base"

namespace loofah
{

PollerBase::PollerBase() noexcept
{
    _io_thread_tid = std::this_thread::get_id();
}

void PollerBase::add_later_task(task_type&& task) noexcept
{
    _later_tasks.eliminate_enqueue(std::forward<task_type>(task));
}

void PollerBase::add_later_task(const task_type& task) noexcept
{
    _later_tasks.eliminate_enqueue(task);
}

bool PollerBase::is_in_io_thread() const noexcept
{
    return _io_thread_tid == std::this_thread::get_id();
}

bool PollerBase::is_in_io_thread_and_not_polling() const noexcept
{
    return PollStage::NotPolling == _poll_stage && is_in_io_thread();
}

void PollerBase::run_later_tasks() noexcept
{
    // NOTE This method can only be called from inside IO thread
    assert(is_in_io_thread_and_not_polling());

    task_type task;
    while (_later_tasks.eliminate_dequeue(&task))
        task();
}

void PollerBase::run_later(task_type&& task) noexcept
{
    const bool in_iothread = is_in_io_thread();
    if (in_iothread && PollStage::NotPolling == _poll_stage)
    {
        task();
        return;
    }

    add_later_task(std::forward<task_type>(task));

    if (!in_iothread)
        wakeup_poll_wait();
}

void PollerBase::run_later(const task_type& task) noexcept
{
    const bool in_iothread = is_in_io_thread();
    if (in_iothread && PollStage::NotPolling == _poll_stage)
    {
        task();
        return;
    }

    add_later_task(task);

    if (!in_iothread)
        wakeup_poll_wait();
}

}
