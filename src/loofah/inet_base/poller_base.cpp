
#include "../loofah_config.h"

#include <nut/logging/logger.h>

#include "poller_base.h"


#define TAG "loofah.poller_base"

namespace loofah
{

void PollerBase::initialize_base() noexcept
{
    assert(State::Uninitialized == _state.load(std::memory_order_relaxed));

    _poller_tid = std::this_thread::get_id();
    _state.store(State::Polling, std::memory_order_release);
}

bool PollerBase::is_in_poll_thread() const noexcept
{
    assert(State::Uninitialized != _state.load(std::memory_order_relaxed));

    return _poller_tid == std::this_thread::get_id();
}

void PollerBase::run_in_poll_thread(task_type&& task) noexcept
{
    if (is_in_poll_thread())
    {
        task();
        return;
    }

    _deferred_tasks.eliminate_enqueue(std::forward<task_type>(task));
    wakeup_poll_wait();
}

void PollerBase::run_in_poll_thread(const task_type& task) noexcept
{
    if (is_in_poll_thread())
    {
        task();
        return;
    }

    _deferred_tasks.eliminate_enqueue(task);
    wakeup_poll_wait();
}

void PollerBase::run_deferred_tasks() noexcept
{
    // NOTE This method can only be called from inside polling thread
    assert(is_in_poll_thread());

    task_type task;
    while (_deferred_tasks.eliminate_dequeue(&task))
        task();
}

}
