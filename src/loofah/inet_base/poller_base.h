
#ifndef ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_
#define ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_

#include "../loofah_config.h"

#include <vector>
#include <functional>
#include <thread>

#include <nut/threading/lockfree/concurrent_queue.h>


namespace loofah
{

class LOOFAH_API PollerBase
{
public:
    typedef std::function<void()> task_type;

public:
    PollerBase() = default;
    virtual ~PollerBase() = default;

    /**
     * 当前上下文是否在轮询线程中
     *
     * NOTE:
     * - reactor 的下列操作需要放到轮询线程中:
     *   register_handler()
     *   unregister_handler()
     *   enable_handler()
     *   disable_handler()
     *   poll()
     *   shutdown()
     *
     * - proactor 的下列操作需要放到轮询线程中:
     *   register_handler()
     *   unregister_handler()
     *   launch_accept()
     *   launch_connect()
     *   launch_read()
     *   launch_write()
     *   poll()
     *   shutdown()
     */
    bool is_in_poll_thread() const noexcept;

    /**
     * 在轮询线程中运行
     */
    void run_in_poll_thread(task_type&& task) noexcept;
    void run_in_poll_thread(const task_type& task) noexcept;

    /**
     * 如果轮询线程处于轮询等待状态(select() / WSAPoll() / kevent() / epoll_wait())
     * 则唤醒轮询线程
     *
     * NOTE 该方法可以从非轮询线程调用
     */
    virtual void wakeup_poll_wait() noexcept = 0;

protected:
    /**
     * 初始化
     */
    void initialize_base() noexcept;

    /**
     * 运行并清空所有异步任务
     *
     * NOTE This method can only be called from inside polling thread
     */
    void run_deferred_tasks() noexcept;

private:
    PollerBase(const PollerBase&) = delete;
    PollerBase& operator=(const PollerBase&) = delete;

protected:
    enum class State
    {
        Uninitialized, // 未初始化
        Polling, // 轮询中
        Shutdown // 已关闭
    };
    std::atomic<State> _state = ATOMIC_VAR_INIT(State::Uninitialized);

private:
    std::thread::id _poller_tid; // Initial invalid id
    nut::ConcurrentQueue<task_type> _deferred_tasks;
};

}

#endif
