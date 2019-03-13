
#include "../loofah_config.h"

#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>

#include "react_package_channel.h"
#include "../inet_base/error.h"


#define TAG "loofah.package.react_package_channel"

namespace loofah
{

void ReactPackageChannel::set_reactor(Reactor *reactor)
{
    assert(nullptr != reactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _actor);
    _actor = reactor;
}

Reactor* ReactPackageChannel::get_reactor() const
{
    return (Reactor*) _actor;
}

SockStream& ReactPackageChannel::get_sock_stream()
{
    return _sock_stream;
}

void ReactPackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(_sock_stream.is_null());

    ReactChannel::open(fd);

    Reactor *reactor = (Reactor*) _actor;
    reactor->register_handler(this, ReactHandler::READ_MASK | ReactHandler::WRITE_MASK);
    reactor->disable_handler(this, ReactHandler::WRITE_MASK);
}

void ReactPackageChannel::close(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // 设置关闭标记
    _closing = true;

    // 直接强制关闭
    if (discard_write || 0 == LOOFAH_FORCE_CLOSE_DELAY ||
        _pkg_write_queue.empty() || _sock_stream.is_writing_shutdown())
    {
        force_close();
        return;
    }

    // 先关闭读通道
    ((Reactor*) _actor)->disable_handler(this, ReactHandler::READ_MASK);
    _sock_stream.shutdown_read();

    // 设置超时关闭
    setup_force_close_timer();
}

void ReactPackageChannel::force_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(_closing);

    if (_sock_stream.is_null())
        return;

    // Do close
    cancel_force_close_timer();
    ((Reactor*) _actor)->unregister_handler(this);
    _sock_stream.close();

    // Handle close event
    if (_actor->is_in_loop_thread_and_not_handling())
    {
        // Synchronize
        handle_close(); // NOTE 这里可能会导致自身被删除, 放到最后
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<ReactPackageChannel> ref_this(this);
        _actor->run_later([=] { ref_this->handle_close(); });
    }
}

void ReactPackageChannel::handle_read_ready()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // Ignore if in closing
    Reactor *reactor = (Reactor*) _actor;
    while (!_closing)
    {
        if (nullptr == _reading_pkg)
        {
            _reading_pkg = nut::rc_new<Package>(LOOFAH_INIT_READ_PKG_SIZE);
            _reading_pkg->raw_rewind();
        }
        ssize_t rs = _sock_stream.read(_reading_pkg->writable_data(), _reading_pkg->writable_size());
        if (0 == rs)
        {
            // Read channel closed
            _sock_stream.mark_reading_shutdown();
            reactor->disable_handler(this, ReactHandler::READ_MASK);
            handle_reading_shutdown();
            return;
        }
        else if (LOOFAH_ERR_WOULD_BLOCK == rs)
        {
            // All read, next reading will be blocked
            return;
        }
        else if (rs < 0)
        {
            // Error
            _sock_stream.mark_reading_shutdown();
            reactor->disable_handler(this, ReactHandler::READ_MASK);
            handle_error(rs);
            return;
        }

        split_and_handle_packages((size_t) rs);
    }
}

void ReactPackageChannel::write(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(!_sock_stream.is_null());

    if (_closing || _sock_stream.is_writing_shutdown())
    {
        if (_closing)
            NUT_LOG_E(TAG, "channel is closing, writing package discard. fd %d", get_socket());
        else
            NUT_LOG_E(TAG, "write channel is closed, writing package discard. fd %d", get_socket());
        return;
    }

    pkg->raw_pack();
    _pkg_write_queue.push_back(pkg);
    if (1 == _pkg_write_queue.size())
        ((Reactor*) _actor)->enable_handler(this, ReactHandler::WRITE_MASK);
}

void ReactPackageChannel::handle_write_ready()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // NOTE '_closing' 可能为 true, 做关闭前最后的写入

    Reactor *reactor = (Reactor*) _actor;
    while (!_pkg_write_queue.empty() && !_sock_stream.is_writing_shutdown())
    {
#if NUT_PLATFORM_OS_MAX || NUT_PLATFORM_OS_LINUX
        if (1 == _pkg_write_queue.size())
        {
#endif
            Package *pkg = _pkg_write_queue.front();
            assert(nullptr != pkg);
            const size_t readable = pkg->readable_size();
            const ssize_t rs = _sock_stream.write(pkg->readable_data(), readable);
            if (rs >= 0)
            {
                assert(rs <= (ssize_t) readable);
                if (rs == (ssize_t) readable)
                    _pkg_write_queue.pop_front();
                else
                    pkg->skip_read(rs);
            }
            else if (LOOFAH_ERR_WOULD_BLOCK == rs)
            {
                // Next writing will be blocked
                break;
            }
            else
            {
                // Error
                _sock_stream.mark_writing_shutdown();
                reactor->disable_handler(this, ReactHandler::WRITE_MASK);
                if (!_closing)
                {
                    handle_error(rs);
                    return;
                }
                break;
            }
#if NUT_PLATFORM_OS_MAX || NUT_PLATFORM_OS_LINUX
        }
        else
        {
            void *bufs[LOOFAH_STACK_BUFFER_LENGTH];
            size_t lens[LOOFAH_STACK_BUFFER_LENGTH];
            const size_t buf_count = std::min((size_t) LOOFAH_STACK_BUFFER_LENGTH, _pkg_write_queue.size());

            queue_t::const_iterator iter = _pkg_write_queue.begin();
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
                assert(!_pkg_write_queue.empty());
                Package *pkg = _pkg_write_queue.front();
                assert(nullptr != pkg);
                const size_t readable = pkg->readable_size();
                if (rs >= (ssize_t) readable)
                {
                    _pkg_write_queue.pop_front();
                    rs -= readable;
                }
                else
                {
                    pkg->skip_read(rs);
                    rs = 0;
                }
            }
            if (LOOFAH_ERR_WOULD_BLOCK == rs)
            {
                // Next writing will be blocked
                break;
            }
            else if (rs < 0)
            {
                // Error
                _sock_stream.mark_writing_shutdown();
                reactor->disable_handler(this, ReactHandler::WRITE_MASK);
                if (!_closing)
                {
                    handle_error(rs);
                    return;
                }
                break;
            }
        }
#endif
    }

    if (_pkg_write_queue.empty() || _sock_stream.is_writing_shutdown())
    {
        // 如果本地写队列空了，关闭写
        reactor->disable_handler(this, ReactHandler::WRITE_MASK);

        // 如果本地写队列空了，并且处于关闭流程中，则关闭 socket
        if (_closing)
        {
            if (_sock_stream.is_reading_shutdown())
                force_close();
            else if (!_sock_stream.is_writing_shutdown())
                _sock_stream.shutdown_write();
        }
    }
}

void ReactPackageChannel::handle_exception(int err)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    if (LOOFAH_ERR_CONNECTION_RESET == err)
        _sock_stream.mark_writing_shutdown();

    if (!_closing)
        handle_error(err);
}

}
