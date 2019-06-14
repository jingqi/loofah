
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
    PollerBase() noexcept;
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
     * 运行并清空所有异步任务
     *
     * NOTE This method can only be called from inside IO thread
     */
    void run_deferred_tasks() noexcept;

    /**
     * 添加一个异步任务
     */
    void add_task(task_type&& task) noexcept;
    void add_task(const task_type& task) noexcept;

private:
    PollerBase(const PollerBase&) = delete;
    PollerBase& operator=(const PollerBase&) = delete;

private:
    std::thread::id _poller_tid;
    nut::ConcurrentQueue<task_type> _deferred_tasks;
};

}

#endif
