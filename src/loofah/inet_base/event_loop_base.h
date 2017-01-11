
#ifndef ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_
#define ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_

#include "../loofah_config.h"

#include <vector>
#include <functional>

#include <nut/threading/thread.h>
#include <nut/threading/sync/mutex.h>


namespace loofah
{

class LOOFAH_API EventLoopBase
{
private:
    nut::Thread::tid_type _loop_tid;
    bool _in_handling_events = false;
    std::vector<std::function<void()> > _later_tasks;
    nut::Mutex _mutex;

protected:
    class HandleEventsGuard
    {
        EventLoopBase *_loop;

    public:
        HandleEventsGuard(EventLoopBase *loop)
            : _loop(loop)
        {
            loop->_in_handling_events = true;
        }

        ~HandleEventsGuard()
        {
            _loop->_in_handling_events = false;
        }
    };

    /**
     * 运行并清空所有异步任务
     *
     * NOTE This method can only be called from inside loop thread
     */
    void run_later_tasks();

    /**
     * 添加一个异步任务
     */
    void add_later_task(const std::function<void()>& task);

public:
    EventLoopBase();

    /**
     * 当前是否在事件处理线程中
     */
    bool is_in_loop_thread() const;

    /**
     * 当前线程是否是事件循环线程
     */
    bool is_in_loop_thread_and_not_handling() const
    {
        return !_in_handling_events && is_in_loop_thread();
    }

    /**
     * 在事件循环线程中运行
     */
    void run_later(const std::function<void()>& task);
};

}

#endif
