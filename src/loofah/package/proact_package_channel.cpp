
#include "../loofah_config.h"

#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>

#include "proact_package_channel.h"
#include "../inet_base/error.h"


#define TAG "loofah.package.proact_package_channel"

namespace loofah
{

void ProactPackageChannel::set_proactor(Proactor *proactor)
{
    assert(nullptr != proactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _actor);
    _actor = proactor;
}

Proactor* ProactPackageChannel::get_proactor() const
{
    return (Proactor*) _actor;
}

SockStream& ProactPackageChannel::get_sock_stream()
{
    return _sock_stream;
}

void ProactPackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(_sock_stream.is_null());

    ProactChannel::open(fd);

    ((Proactor*) _actor)->register_handler(this);
    launch_read();
}

void ProactPackageChannel::close(bool discard_write)
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
    _sock_stream.shutdown_read();

    // 设置超时关闭
    setup_force_close_timer();
}

void ProactPackageChannel::force_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(_closing);

    if (_sock_stream.is_null())
        return;

    // Do close
    cancel_force_close_timer();
    ((Proactor*) _actor)->unregister_handler(this);
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
        nut::rc_ptr<ProactPackageChannel> ref_this(this);
        _actor->run_later([=] { ref_this->handle_close(); });
    }
}

void ProactPackageChannel::launch_read()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(!_sock_stream.is_null() && !_sock_stream.is_reading_shutdown());
    assert(!_closing);

    if (nullptr == _reading_pkg)
    {
        _reading_pkg = nut::rc_new<Package>(LOOFAH_INIT_READ_PKG_SIZE);
        _reading_pkg->raw_rewind();
    }

    void *const buf = _reading_pkg->writable_data();
    const size_t buf_cap = _reading_pkg->writable_size();
    ((Proactor*) _actor)->launch_read(this, &buf, &buf_cap, 1);
}

void ProactPackageChannel::handle_read_completed(size_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(nullptr != _reading_pkg);

    if (0 == cb)
    {
        // Read channel closed
        const bool old_reading_shutdown = _sock_stream.is_reading_shutdown();
        _sock_stream.mark_reading_shutdown();
        if (!_closing && !old_reading_shutdown)
            handle_reading_shutdown();
        return;
    }

    if (!_closing && !_sock_stream.is_reading_shutdown())
        split_and_handle_packages(cb);

    if (!_closing && !_sock_stream.is_reading_shutdown())
        launch_read();
}

void ProactPackageChannel::write(Package *pkg)
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
        launch_write();
}

void ProactPackageChannel::launch_write()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(!_sock_stream.is_null() && !_sock_stream.is_writing_shutdown());

    // NOTE '_closing' 可能为 true, 做关闭前最后的写入

    // NOTE 写入前需要检查 RST 错误
    const int err = _sock_stream.get_last_error();
    if (0 != err)
    {
        handle_exception(err);
        return;
    }

    assert(!_pkg_write_queue.empty());
    void* bufs[LOOFAH_STACK_BUFFER_LENGTH];
    size_t lens[LOOFAH_STACK_BUFFER_LENGTH];
    const size_t buf_count = std::min((size_t) LOOFAH_STACK_BUFFER_LENGTH, _pkg_write_queue.size());

    queue_t::const_iterator iter = _pkg_write_queue.begin();
    for (size_t i = 0; i < buf_count; ++i, ++iter)
    {
        Package *pkg = *iter;
        assert(nullptr != pkg);
        bufs[i] = (void*) pkg->readable_data(); // NOTE (const void*) 转成 (void*) 只是为了方便传入参数
        lens[i] = pkg->readable_size();
    }

    assert(nullptr != _actor);
    ((Proactor*) _actor)->launch_write(this, bufs, lens, buf_count);
}

void ProactPackageChannel::handle_write_completed(size_t cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // NOTE '_closing' 可能为 true, 做关闭前最后的写入

    // 从本地写队列中移除已写内容
    while (cb > 0)
    {
        assert(!_pkg_write_queue.empty());
        Package *pkg = _pkg_write_queue.front();
        assert(nullptr != pkg);
        const size_t readable = pkg->readable_size();
        if (cb >= readable)
        {
            _pkg_write_queue.pop_front();
            cb -= readable;
        }
        else
        {
            pkg->skip_read(cb);
            cb = 0;
        }
    }

    // 如果本地写队列中还有内容，继续写
    if (!_pkg_write_queue.empty() && !_sock_stream.is_writing_shutdown())
    {
        launch_write();
        return;
    }

    // 如果本地写队列空了，并且处于关闭流程中，则关闭 socket
    if (_closing)
    {
        if (_sock_stream.is_reading_shutdown())
            force_close();
        else if (!_sock_stream.is_writing_shutdown())
            _sock_stream.shutdown_write();
    }
}

void ProactPackageChannel::handle_exception(int err)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    if (LOOFAH_ERR_CONNECTION_RESET == err)
        _sock_stream.mark_writing_shutdown();

    if (_closing)
        force_close();
    else
        handle_error(err);
}

}
