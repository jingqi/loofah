
#ifndef ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_
#define ___HEADFILE_AE9D2E29_0D2B_4441_AEE9_A287CFD2C8AD_

#include "../loofah_config.h"

#include <vector>

#include <nut/threading/runnable.h>
#include <nut/threading/thread.h>
#include <nut/threading/sync/mutex.h>


namespace loofah
{

class LOOFAH_API EventLoopBase
{
private:
    nut::Thread::tid_type _loop_tid;
    std::vector<nut::rc_ptr<nut::Runnable> > _async_tasks;
    nut::Mutex _mutex;

protected:
    /**
     * 运行并清空所有异步任务
     *
     * NOTE This method can only be called from inside loop thread
     */
    void run_async_tasks();

    /**
     * 添加一个异步任务
     */
    void add_async_task(nut::Runnable *runnable);

public:
    EventLoopBase();

    /**
     * 当前线程是否是事件循环线程
     */
    bool is_in_loop_thread() const;

    /**
     * 在事件循环线程中运行
     */
    void run_in_loop_thread(nut::Runnable *runnable);
};

}

#endif
