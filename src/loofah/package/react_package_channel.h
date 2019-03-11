
#ifndef ___HEADFILE_54F36F61_E321_40AB_A45B_61513B51FB12_
#define ___HEADFILE_54F36F61_E321_40AB_A45B_61513B51FB12_

#include "../loofah_config.h"

#include <list>

#include <nut/time/time_wheel.h>
#include <nut/container/rwbuffer/fragment_buffer.h>
#include <nut/debugging/destroy_checker.h>

#include "../reactor/react_channel.h"
#include "../reactor/reactor.h"
#include "package.h"


namespace loofah
{

class LOOFAH_API ReactPackageChannel : public ReactChannel
{
public:
    virtual ~ReactPackageChannel();

    void set_reactor(Reactor *reactor);
    Reactor* get_reactor() const;

    void set_time_wheel(nut::TimeWheel *time_wheel);
    nut::TimeWheel* get_time_wheel() const;

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
    void write(Package *pkg);
    void write_later(Package *pkg);

    /**
     * 关闭连接
     *
     * @param discard_write 是否忽略尚未写入的 package, 否则等待全部写入后再关闭
     */
    void close_later(bool discard_write = false);

public:
    /**
     * ReactChannel 接口实现
     */
    virtual void open(socket_t fd) final override;
    virtual void handle_read_ready() final override;
    virtual void handle_write_ready() final override;

private:
    // 关闭连接
    void try_close(bool discard_write);
    void do_close();

    // 定时强制关闭
    void setup_force_close_timer();
    void cancel_force_close_timer();

private:
    Reactor *_reactor = nullptr;
    nut::TimeWheel *_time_wheel = nullptr;

    typedef std::list<nut::rc_ptr<Package> > queue_t;
    queue_t _write_queue; // 写队列

    nut::FragmentBuffer _readed_buffer; // 已经读取的数据

    nut::TimeWheel::timer_id_type _force_close_timer = NUT_INVALID_TIMER_ID;
    bool _closing = false; // 是否等待关闭

    NUT_DEBUGGING_DESTROY_CHECKER
};

}

#endif
