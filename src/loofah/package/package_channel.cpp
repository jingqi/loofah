
#include "../loofah_config.h"

#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>

#include "package_channel.h"


// 读缓冲区大小
#define READ_BUF_LEN 65536
// 栈上 buffer 指针数
#define STACK_BUF_COUNT 10
// 强制关闭超时事件, 单位为毫秒. <0 表示不强制关闭, 0 表示立即关闭, >0 表示超时强制关闭
#define FORCE_CLOSE_DELAY 5000

#define TAG "loofah.package_channel"

namespace loofah
{

PackageChannel::~PackageChannel()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    cancel_force_close_timer();

    if (nullptr != _read_frag)
        nut::FragmentBuffer::delete_fragment(_read_frag);
    _read_frag = nullptr;
}

void PackageChannel::set_proactor(Proactor *proactor)
{
    assert(nullptr != proactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _proactor);
    _proactor = proactor;
}

Proactor* PackageChannel::get_proactor() const
{
    return _proactor;
}

void PackageChannel::set_time_wheel(nut::TimeWheel *time_wheel)
{
    assert(nullptr != time_wheel);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _time_wheel);
    _time_wheel = time_wheel;
}

nut::TimeWheel* PackageChannel::get_time_wheel() const
{
    return _time_wheel;
}

void PackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread() &&
           !_sock_stream.is_valid());

    ProactChannel::open(fd);

    _proactor->register_handler_later(this);
    launch_read();
}

void PackageChannel::setup_force_close_timer()
{
    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID != _force_close_timer ||
        FORCE_CLOSE_DELAY <= 0)
        return;

    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    _force_close_timer = _time_wheel->add_timer(
        FORCE_CLOSE_DELAY, 0,
        [=](nut::TimeWheel::timer_id_type id, uint64_t expires)
        {
            _force_close_timer = NUT_INVALID_TIMER_ID;
            force_close();
        });
}

void PackageChannel::cancel_force_close_timer()
{
    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID == _force_close_timer)
        return;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    _time_wheel->cancel_timer(_force_close_timer);
    _force_close_timer = NUT_INVALID_TIMER_ID;
}

void PackageChannel::force_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    // 设置关闭标记
    _closing = true;

    if (!_sock_stream.is_valid())
        return;
    cancel_force_close_timer();
    _sock_stream.close();

    // Handle close event
    if (_proactor->is_in_loop_thread_and_not_handling())
    {
        handle_close(); // NOTE 这里可能会导致自身被删除, 放到最后
        return;
    }

    nut::rc_ptr<PackageChannel> ref_this(this);
    _proactor->run_later([=] { ref_this->handle_close(); });
}

void PackageChannel::start_closing()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    // 设置关闭标记
    _closing = true;

    // 可能需要强制关闭
    if (0 == FORCE_CLOSE_DELAY || _write_queue.empty())
    {
        force_close();
        return;
    }

    // 关闭读通道
    _sock_stream.shutdown_read();

    // 设置超时关闭
    setup_force_close_timer();
}

void PackageChannel::close_later()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    // 设置关闭标记
    _closing = true;

    assert(nullptr != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        start_closing();
        return;
    }

    nut::rc_ptr<PackageChannel> ref_this(this);
    _proactor->run_later([=] { ref_this->start_closing(); });
}

void PackageChannel::launch_read()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread() &&
           _sock_stream.is_valid() && !_sock_stream.is_reading_shutdown());

    if (nullptr == _read_frag)
        _read_frag = nut::FragmentBuffer::new_fragment(READ_BUF_LEN);
    void *buf = _read_frag->buffer;
    _proactor->launch_read_later(this, &buf, &_read_frag->capacity, 1);
}

void PackageChannel::launch_write()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread() &&
           _sock_stream.is_valid() && !_sock_stream.is_writing_shutdown());

    assert(!_write_queue.empty());
    void *bufs[STACK_BUF_COUNT];
    size_t lens[STACK_BUF_COUNT];
    const size_t buf_count = (std::min)((size_t) STACK_BUF_COUNT, _write_queue.size());

    queue_t::const_iterator iter = _write_queue.begin();
    for (size_t i = 0; i < buf_count; ++i, ++iter)
    {
        Package *pkg = *iter;
        assert(nullptr != pkg);
        bufs[i] = (void*) pkg->readable_data();
        lens[i] = pkg->readable_size();
    }

    assert(nullptr != _proactor);
    _proactor->launch_write_later(this, bufs, lens, buf_count);
}

void PackageChannel::write(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread() &&
           _sock_stream.is_valid() && !_closing);

    uint32_t header = pkg->readable_size();
    pkg->prepend(&header);
    _write_queue.push_back(pkg);
    if (1 == _write_queue.size())
        launch_write();
}

void PackageChannel::write_later(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (_closing)
    {
        NUT_LOG_E(TAG, "should not write to a closing channel");
        return;
    }

    assert(nullptr != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        write(pkg);
        return;
    }

    nut::rc_ptr<PackageChannel> ref_this(this);
    nut::rc_ptr<Package> ref_pkg(pkg);
    _proactor->run_later([=] { ref_this->write(ref_pkg); });
}

void PackageChannel::handle_read_completed(ssize_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (0 == cb)
    {
        _closing = true;
        _sock_stream.set_reading_shutdown();
        if (0 == FORCE_CLOSE_DELAY || _sock_stream.is_writing_shutdown() || _write_queue.empty())
        {
            force_close();
            return;
        }

        setup_force_close_timer();
        return;
    }

    // Ignore if in closing
    if (_closing)
        return;

    // Cache to buffer
    assert(cb <= (ssize_t) _read_frag->capacity);
    _read_frag->size = cb;
    _read_frag = _readed_buffer.write_fragment(_read_frag);
    launch_read();

    // Read package
    const size_t readable = _readed_buffer.readable_size();
    if (readable < sizeof(uint32_t))
        return;
    uint32_t pkg_len = 0;
    const size_t rs = _readed_buffer.look_ahead(&pkg_len, sizeof(pkg_len));
    assert(sizeof(pkg_len) == rs);
    UNUSED(rs);
    if (sizeof(pkg_len) + pkg_len > readable)
        return;

    // Handle package
    nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
    assert(pkg->writable_size() >= pkg_len);
    _readed_buffer.skip_read(sizeof(pkg_len));
    _readed_buffer.read(pkg->writable_data(), pkg_len);
    pkg->skip_write(pkg_len);
    handle_read(pkg);
}

void PackageChannel::handle_write_completed(ssize_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    // 从本地写队列中移除已写内容
    assert(!_write_queue.empty());
    while (cb > 0)
    {
        nut::rc_ptr<Package> pkg = _write_queue.front();
        const size_t readable = pkg->readable_size();
        if (cb >= (ssize_t) readable)
        {
            _write_queue.pop_front();
            cb -= readable;
        }
        else
        {
            pkg->skip_read(cb);
            cb = 0;
        }
    }

    // 如果本地写队列中还有内容，继续写
    if (!_write_queue.empty())
    {
        launch_write();
        return;
    }

    // 如果本地写队列空了，并且处于关闭流程中，则关闭
    if (_closing)
    {
        if (_sock_stream.is_reading_shutdown())
            force_close();
        else
            _sock_stream.shutdown_write();
        return;
    }
}

}
