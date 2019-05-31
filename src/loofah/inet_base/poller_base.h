
#ifndef ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_
#define ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_

#include "../loofah_config.h"

#include <vector>
#include <functional>
#include <mutex>
#include <thread>


namespace loofah
{

class LOOFAH_API PollerBase
{
public:
    typedef std::function<void()> task_type;

public:
    PollerBase();

    /**
     * 当前是否运行在 IO 线程中
     *
     * reactor 的下列操作需要放到满足该条件:
     *   register_handler()
     *   unregister_handler()
     *   enable_handler()
     *   disable_handler()
     *   shutdown()
     *
     * proactor 的下列操作需要放到满足该条件:
     *   register_handler()
     *   unregister_handler()
     *   launch_accept()
     *   launch_read()
     *   launch_write()
     *   shutdown()
     */
    bool is_in_io_thread() const;

    /**
     * 当前是否运行在 IO 线程中, 并且处于 poll 间隔
     *
     * ReactHandler, ProactHandler 的析构，必须满足该条件
     */
    bool is_in_io_thread_and_not_polling() const;

    /**
     * 在事件循环线程且事件处理间隔中运行
     */
    void run_later(task_type&& task);
    void run_later(const task_type& task);

protected:
    /**
     * 运行并清空所有异步任务
     *
     * NOTE This method can only be called from inside IO thread
     */
    void run_later_tasks();

    /**
     * 添加一个异步任务
     */
    void add_later_task(task_type&& task);
    void add_later_task(const task_type& task);

    class PollingGuard
    {
        PollerBase *_poller;

    public:
        PollingGuard(PollerBase *poller)
            : _poller(poller)
        {
            poller->_in_polling = true;
        }

        ~PollingGuard()
        {
            _poller->_in_polling = false;
        }

    private:
        PollingGuard(const PollingGuard&) = delete;
        PollingGuard& operator=(const PollingGuard&) = delete;
    };

private:
    PollerBase(const PollerBase&) = delete;
    PollerBase& operator=(const PollerBase&) = delete;

private:
    std::thread::id _io_tid;
    bool _in_polling = false;
    std::vector<task_type> _later_tasks;
    std::mutex _mutex;
};

}

#endif
