
#include "../loofah_config.h"

#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>
#include <nut/platform/endian.h>

#include "proact_package_channel.h"


// 读缓冲区大小
#define READ_BUF_LEN 65536
// 栈上 buffer 指针数
#define STACK_BUF_COUNT 10
// 强制关闭超时事件, 单位为毫秒. <0 表示不强制关闭, 0 表示立即关闭, >0 表示超时强制关闭
#define FORCE_CLOSE_DELAY 5000

#define TAG "loofah.proact_package_channel"

namespace loofah
{

ProactPackageChannel::~ProactPackageChannel()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    cancel_force_close_timer();

    if (nullptr != _reading_frag)
        nut::FragmentBuffer::delete_fragment(_reading_frag);
    _reading_frag = nullptr;
}

void ProactPackageChannel::set_proactor(Proactor *proactor)
{
    assert(nullptr != proactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _proactor);
    _proactor = proactor;
}

Proactor* ProactPackageChannel::get_proactor() const
{
    return _proactor;
}

void ProactPackageChannel::set_time_wheel(nut::TimeWheel *time_wheel)
{
    assert(nullptr != time_wheel);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _time_wheel);
    _time_wheel = time_wheel;
}

nut::TimeWheel* ProactPackageChannel::get_time_wheel() const
{
    return _time_wheel;
}

void ProactPackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(!_sock_stream.is_valid());

    ProactChannel::open(fd);

    _proactor->register_handler(this);
    launch_read();
}

void ProactPackageChannel::close_later(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    // 设置关闭标记
    _closing = true;

    assert(nullptr != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        // Synchronize
        try_close(discard_write);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactPackageChannel> ref_this(this);
        _proactor->run_later([=] { ref_this->try_close(discard_write); });
    }
}

void ProactPackageChannel::try_close(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(_closing);

    // 直接强制关闭
    if (discard_write || 0 == FORCE_CLOSE_DELAY || _write_queue.empty())
    {
        do_close();
        return;
    }

    // 先关闭读通道
    _sock_stream.shutdown_read();

    // 设置超时关闭
    setup_force_close_timer();
}

void ProactPackageChannel::do_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(_closing);

    if (!_sock_stream.is_valid())
        return;

    cancel_force_close_timer();

    _proactor->unregister_handler(this);
    _sock_stream.close();

    // Handle close event
    if (_proactor->is_in_loop_thread_and_not_handling())
    {
        // Synchronize
        handle_close(); // NOTE 这里可能会导致自身被删除, 放到最后
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactPackageChannel> ref_this(this);
        _proactor->run_later([=] { ref_this->handle_close(); });
    }
}

void ProactPackageChannel::setup_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID != _force_close_timer ||
        FORCE_CLOSE_DELAY <= 0)
        return;

    _force_close_timer = _time_wheel->add_timer(
        FORCE_CLOSE_DELAY, 0,
        [=] (nut::TimeWheel::timer_id_type id, uint64_t expires) {
            _force_close_timer = NUT_INVALID_TIMER_ID;
            do_close();
        });
}

void ProactPackageChannel::cancel_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID == _force_close_timer)
        return;
    _time_wheel->cancel_timer(_force_close_timer);
    _force_close_timer = NUT_INVALID_TIMER_ID;
}

void ProactPackageChannel::launch_read()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(_sock_stream.is_valid() && !_sock_stream.is_reading_shutdown());

    if (nullptr == _reading_frag)
        _reading_frag = nut::FragmentBuffer::new_fragment(READ_BUF_LEN);
    void *buf = _reading_frag->buffer;
    _proactor->launch_read(this, &buf, &_reading_frag->capacity, 1);
}

void ProactPackageChannel::handle_read_completed(ssize_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    // Ignore if in closing
    if (_closing)
        return;

    if (0 == cb)
    {
        // Read channel closed
        _sock_stream.set_reading_shutdown();
        handle_reading_shutdown();
        return;
    }
    else if (cb < 0)
    {
        // Error
        handle_exception();
        return;
    }

    // Cache to buffer
    assert(cb <= (ssize_t) _reading_frag->capacity);
    _reading_frag->size = cb;
    _reading_frag = _readed_buffer.write_fragment(_reading_frag);
    launch_read();

    // Read package
    while (true)
    {
        const size_t readable = _readed_buffer.readable_size();
        if (readable < sizeof(uint32_t))
            return;
        uint32_t pkg_len = 0;
        const size_t rs = _readed_buffer.look_ahead(&pkg_len, sizeof(uint32_t));
        assert(sizeof(uint32_t) == rs);
        UNUSED(rs);
        pkg_len = be32toh(pkg_len);
        if (sizeof(pkg_len) + pkg_len > readable)
            return;

        // Handle package
        nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
        assert(pkg->writable_size() >= pkg_len);
        _readed_buffer.skip_read(sizeof(uint32_t));
        _readed_buffer.read(pkg->writable_data(), pkg_len);
        pkg->skip_write(pkg_len);
        handle_read(pkg);
    }
}

void ProactPackageChannel::handle_reading_shutdown()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    close_later();
}

void ProactPackageChannel::handle_exception()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());

    NUT_LOG_E(TAG, "error in socket");
    close_later();
}

void ProactPackageChannel::write_later(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (_closing)
    {
        NUT_LOG_E(TAG, "channel is closing, writing package discard");
        return;
    }

    assert(nullptr != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        // Synchronize
        write(pkg);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ProactPackageChannel> ref_this(this);
        nut::rc_ptr<Package> ref_pkg(pkg);
        _proactor->run_later([=] { ref_this->write(ref_pkg); });
    }
}

void ProactPackageChannel::write(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(_sock_stream.is_valid() && !_closing);

    if (_closing)
    {
        NUT_LOG_E(TAG, "channel is closing, writing package discard");
        return;
    }

    pkg->pack();
    _write_queue.push_back(pkg);
    if (1 == _write_queue.size())
        launch_write();
}

void ProactPackageChannel::launch_write()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _proactor && _proactor->is_in_loop_thread());
    assert(_sock_stream.is_valid() && !_sock_stream.is_writing_shutdown());

    assert(!_write_queue.empty());
    void *bufs[STACK_BUF_COUNT];
    size_t lens[STACK_BUF_COUNT];
    const size_t buf_count = std::min((size_t) STACK_BUF_COUNT, _write_queue.size());

    queue_t::const_iterator iter = _write_queue.begin();
    for (size_t i = 0; i < buf_count; ++i, ++iter)
    {
        Package *pkg = *iter;
        assert(nullptr != pkg);
        bufs[i] = (void*) pkg->readable_data();
        lens[i] = pkg->readable_size();
    }

    assert(nullptr != _proactor);
    _proactor->launch_write(this, bufs, lens, buf_count);
}

void ProactPackageChannel::handle_write_completed(ssize_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (cb < 0)
    {
        handle_exception();
        return;
    }

    // 从本地写队列中移除已写内容
    while (cb > 0)
    {
        assert(!_write_queue.empty());
        Package *pkg = _write_queue.front();
        assert(nullptr != pkg);
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
            do_close();
        else
            _sock_stream.shutdown_write();
    }
}

}
