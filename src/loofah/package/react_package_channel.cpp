
#include "../loofah_config.h"

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>
#include <nut/platform/endian.h>

#include "react_package_channel.h"


// 读缓冲区大小
#define READ_BUF_LEN 4096
// 栈上 buffer 指针数
#define STACK_BUF_COUNT 10
// 强制关闭超时事件, 单位为毫秒. <0 表示不强制关闭, 0 表示立即关闭, >0 表示超时强制关闭
#define FORCE_CLOSE_DELAY 5000

#define TAG "loofah.react_package_channel"

namespace loofah
{

ReactPackageChannel::~ReactPackageChannel()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    cancel_force_close_timer();
}

void ReactPackageChannel::set_reactor(Reactor *reactor)
{
    assert(nullptr != reactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _reactor);
    _reactor = reactor;
}

Reactor* ReactPackageChannel::get_reactor() const
{
    return _reactor;
}

void ReactPackageChannel::set_time_wheel(nut::TimeWheel *time_wheel)
{
    assert(nullptr != time_wheel);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _time_wheel);
    _time_wheel = time_wheel;
}

nut::TimeWheel* ReactPackageChannel::get_time_wheel() const
{
    return _time_wheel;
}

void ReactPackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());
    assert(!_sock_stream.is_valid());

    ReactChannel::open(fd);

    _reactor->register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    _reactor->disable_handler(this, ReactHandler::WRITE_MASK);
}

void ReactPackageChannel::close_later(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    // 设置关闭标记
    _closing = true;

    assert(nullptr != _reactor);
    if (_reactor->is_in_loop_thread())
    {
        // Synchronize
        try_close(discard_write);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactPackageChannel> ref_this(this);
        _reactor->run_later([=] { ref_this->try_close(discard_write); });
    }
}

void ReactPackageChannel::try_close(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());
    assert(_closing);

    // 直接强制关闭
    if (discard_write || 0 == FORCE_CLOSE_DELAY || _write_queue.empty())
    {
        do_close();
        return;
    }

    // 先关闭读通道
    _reactor->disable_handler(this, ReactHandler::READ_MASK);
    _sock_stream.shutdown_read();

    // 设置超时关闭
    setup_force_close_timer();
}

void ReactPackageChannel::do_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());
    assert(_closing);

    if (!_sock_stream.is_valid())
        return;

    cancel_force_close_timer();

    _reactor->unregister_handler(this);
    _sock_stream.close();

    // Handle close event
    if (_reactor->is_in_loop_thread_and_not_handling())
    {
        // Synchronize
        handle_close(); // NOTE 这里可能会导致自身被删除, 放到最后
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactPackageChannel> ref_this(this);
        _reactor->run_later([=] { ref_this->handle_close(); });
    }
}

void ReactPackageChannel::setup_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

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

void ReactPackageChannel::cancel_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID == _force_close_timer)
        return;
    _time_wheel->cancel_timer(_force_close_timer);
    _force_close_timer = NUT_INVALID_TIMER_ID;
}

void ReactPackageChannel::handle_read_ready()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

    // Ignore if in closing
    if (_closing)
        return;

    bool has_error = false;
    uint8_t buf[READ_BUF_LEN];
    while (true)
    {
        ssize_t rs = _sock_stream.read(buf, READ_BUF_LEN);
        if (0 == rs)
        {
            // Read channel closed
            _reactor->disable_handler(this, ReactHandler::READ_MASK);
            _sock_stream.set_reading_shutdown();
            break;
        }
        else if (-2 == rs)
        {
            // All read, next reading will be blocked
            break;
        }
        else if (rs < 0)
        {
            // Error
			NUT_LOG_E(TAG, "read error %d, fd %d", rs, get_socket());
            _reactor->disable_handler(this, ReactHandler::READ_MASK);
            has_error = true;
            break;
        }

        // Cache to buffer
        assert(rs <= READ_BUF_LEN);
        _readed_buffer.write(buf, rs);
    }

    // Read package
    while (true)
    {
        const size_t readable = _readed_buffer.readable_size();
        if (readable < sizeof(uint32_t))
            break;
        uint32_t pkg_len = 0;
        const size_t rs = _readed_buffer.look_ahead(&pkg_len, sizeof(uint32_t));
        assert(sizeof(uint32_t) == rs);
        UNUSED(rs);
        pkg_len = be32toh(pkg_len);
        if (sizeof(pkg_len) + pkg_len > readable)
            break;

        // Handle package
        nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
        assert(pkg->writable_size() >= pkg_len);
        _readed_buffer.skip_read(sizeof(uint32_t));
        _readed_buffer.read(pkg->writable_data(), pkg_len);
        pkg->skip_write(pkg_len);
        handle_read(pkg);
    }

    if (has_error)
        handle_exception();
    else if (_sock_stream.is_reading_shutdown())
        handle_reading_shutdown();
}

void ReactPackageChannel::handle_reading_shutdown()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

    close_later();
}

void ReactPackageChannel::handle_exception()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

    NUT_LOG_E(TAG, "error in socket");
    close_later();
}

void ReactPackageChannel::write_later(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (_closing)
    {
        NUT_LOG_E(TAG, "channel is closing, writing package discard");
        return;
    }

    assert(nullptr != _reactor);
    if (_reactor->is_in_loop_thread())
    {
        // Synchronize
        write(pkg);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactPackageChannel> ref_this(this);
        nut::rc_ptr<Package> ref_pkg(pkg);
        _reactor->run_later([=] { ref_this->write(ref_pkg); });
    }
}

void ReactPackageChannel::write(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());
    assert(_sock_stream.is_valid() && !_closing);

    if (_closing)
    {
        NUT_LOG_E(TAG, "channel is closing, writing package discard");
        return;
    }

    pkg->pack();
    _write_queue.push_back(pkg);
    if (1 == _write_queue.size())
        _reactor->enable_handler(this, ReactHandler::WRITE_MASK);
}

void ReactPackageChannel::handle_write_ready()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _reactor && _reactor->is_in_loop_thread());

    while (!_write_queue.empty())
    {
#if NUT_PLATFORM_OS_MAX || NUT_PLATFORM_OS_LINUX
        if (1 == _write_queue.size())
        {
#endif
            Package *pkg = _write_queue.front();
            assert(nullptr != pkg);
            const size_t readable = pkg->readable_size();
            const ssize_t rs = _sock_stream.write(pkg->readable_data(), readable);
            if (rs >= 0)
            {
                assert(rs <= (ssize_t) readable);
                if (rs == (ssize_t) readable)
                    _write_queue.pop_front();
                else
                    pkg->skip_read(rs);
            }
            else if (-2 == rs)
            {
                // Next writing will be blocked
                break;
            }
            else
            {
                // Error
                _reactor->disable_handler(this, ReactHandler::WRITE_MASK);
                handle_exception();
                return;
            }
#if NUT_PLATFORM_OS_MAX || NUT_PLATFORM_OS_LINUX
        }
        else
        {
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
            ssize_t rs = _sock_stream.writev(bufs, lens, buf_count);
            while (rs > 0)
            {
                assert(!_write_queue.empty());
                Package *pkg = _write_queue.front();
                assert(nullptr != pkg);
                const size_t readable = pkg->readable_size();
                if (rs >= (ssize_t) readable)
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
            if (-2 == rs)
            {
                // Next writing will be blocked
                break;
            }
            else if (rs < 0)
            {
                // Error
                _reactor->disable_handler(this, ReactHandler::WRITE_MASK);
                handle_exception();
                return;
            }
        }
#endif
    }

    if (_write_queue.empty())
    {
        // 如果本地写队列空了，关闭写
        _reactor->disable_handler(this, ReactHandler::WRITE_MASK);

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

}
