
#ifndef ___HEADFILE_1E454C4B_8852_41CB_AD02_02E897F5AD53_
#define ___HEADFILE_1E454C4B_8852_41CB_AD02_02E897F5AD53_

#include "../loofah_config.h"

#include <list>

#include <nut/rc/rc_ptr.h>
#include <nut/time/time_wheel.h>
#include <nut/debugging/destroy_checker.h>

#include "package.h"
#include "../inet_base/event_loop_base.h"


namespace loofah
{

class LOOFAH_API PackageChannelBase
{
    NUT_REF_COUNTABLE

public:
    virtual ~PackageChannelBase();

    void set_time_wheel(nut::TimeWheel *time_wheel);
    nut::TimeWheel* get_time_wheel() const;

    void set_max_payload_size(size_t max_size);
    size_t get_max_payload_size() const;

    /**
     * 读到 package
     */
    virtual void handle_read(Package *pkg) = 0;

    /**
     * 读通道关闭，默认关闭链接
     */
    virtual void handle_reading_shutdown();

    /**
     * 异常，默认关闭链接
     */
    virtual void handle_exception();

    /**
     * socket 已关闭
     */
    virtual void handle_close() = 0;

    /**
     * 写数据
     */
    virtual void write(Package *pkg) = 0;
    void write_later(Package *pkg);

    /**
     * 关闭连接
     *
     * @param discard_write 是否忽略尚未写入的 package, 否则等待全部写入后再关闭
     */
    virtual void close(bool discard_write = false) = 0;
    void close_later(bool discard_write = false);

protected:
    /**
     * 从读缓存分包，并触发 handle_read()/headle_exception()
     */
    void split_and_handle_packages(size_t extra_readed);

    // 关闭连接
    virtual void force_close() = 0;

    // 定时强制关闭
    void setup_force_close_timer();
    void cancel_force_close_timer();

protected:
    // 反应器
    EventLoopBase *_actor = nullptr;

    // 写队列
    typedef std::list<nut::rc_ptr<Package>> queue_t;
    queue_t _pkg_write_queue;

    // 读缓存
    nut::rc_ptr<Package> _reading_pkg;

    // 是否等待关闭
    bool _closing = false;

    NUT_DEBUGGING_DESTROY_CHECKER

private:
    // 最大 package payload 大小
    size_t _max_payload_size = LOOFAH_DEFAULT_MAX_PKG_SIZE;

    // 延时强制关闭
    nut::TimeWheel *_time_wheel = nullptr;
    nut::TimeWheel::timer_id_type _force_close_timer = NUT_INVALID_TIMER_ID;
};

}

#endif
