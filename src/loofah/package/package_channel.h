
#ifndef ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_
#define ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_

#include "../loofah_config.h"

#include <list>

#include <nut/container/rwbuffer/fragment_buffer.h>
#include <nut/time/time_wheel.h>
#include <nut/debugging/destroy_checker.h>

#include "../proactor/proact_channel.h"
#include "../proactor/proactor.h"
#include "package.h"


namespace loofah
{

class LOOFAH_API PackageChannel : public ProactChannel
{
public:
    virtual ~PackageChannel();

    void set_proactor(Proactor *proactor);
    Proactor* get_proactor() const;

    void set_time_wheel(nut::TimeWheel *time_wheel);
    nut::TimeWheel* get_time_wheel() const;

    virtual void open(socket_t fd) final override;
    virtual void handle_read_completed(ssize_t cb) final override;
    virtual void handle_write_completed(ssize_t cb) final override;

    virtual void handle_read(Package *pkg) = 0;
    virtual void handle_close() = 0;
    void write_later(Package *pkg);
    void close_later();

private:
    void launch_read();
    void launch_write();
    void write(Package *pkg);

    // 强制关闭
    void force_close();
    // 开始关闭流程
    void start_closing();

    // 定时强制关闭
    void setup_force_close_timer();
    void cancel_force_close_timer();

private:
    Proactor *_proactor = nullptr;
    nut::TimeWheel *_time_wheel = nullptr;

    typedef std::list<nut::rc_ptr<Package> > queue_t;
    queue_t _write_queue;

    nut::FragmentBuffer::Fragment *_read_frag = nullptr;
    nut::FragmentBuffer _readed_buffer;

    nut::TimeWheel::timer_id_type _force_close_timer = NUT_INVALID_TIMER_ID;
    bool _closing = false; // 是否等待关闭

    NUT_DEBUGGING_DESTROY_CHECKER
};

}

#endif
