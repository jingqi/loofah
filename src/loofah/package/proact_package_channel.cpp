
#include "../loofah_config.h"

#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>

#include "../inet_base/error.h"
#include "proact_package_channel.h"


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

SockStream& ProactPackageChannel::get_sock_stream()
{
    return _sock_stream;
}

void ProactPackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(_sock_stream.is_null());

    ProactChannel::open(fd);
}

void ProactPackageChannel::handle_channel_connected()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    ((Proactor*) _actor)->register_handler(this);
    launch_read();

    handle_connected();
}

void ProactPackageChannel::close(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // 设置关闭标记
    _closing.store(true, std::memory_order_relaxed);

    // 如果没有可写数据，关闭写通道，或者关闭连接
    if (discard_write || _pkg_write_queue.empty())
    {
        if (_sock_stream.is_reading_shutdown())
        {
            // 被动关闭连接
            force_close();
            return;
        }

        // 主动关闭连接
        _sock_stream.shutdown_write();
    }

    // 设置超时关闭
    setup_force_close_timer();
}

void ProactPackageChannel::force_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    // 设置关闭标记
    _closing.store(true, std::memory_order_relaxed);

    // 关闭 socket
    if (_sock_stream.is_null())
        return;
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
        _sock_stream.mark_reading_shutdown();
        if (_sock_stream.is_writing_shutdown())
            force_close(); // 主动关闭连接
        else
            close(); // 被动关闭连接
        return;
    }

    split_and_handle_packages(cb);

    launch_read();
}

void ProactPackageChannel::write(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());
    assert(!_sock_stream.is_null());

    if (_closing.load(std::memory_order_relaxed))
    {
        NUT_LOG_W(TAG, "channel is closing or closed, writing package discard. fd %d", get_socket());
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

#if NUT_PLATFORM_OS_MACOS
    // macOS 下可以通过 get_last_error() 来检测 RST 错误
    const int err = _sock_stream.get_last_error();
    if (0 != err)
    {
        handle_io_error(err);
        return;
    }
#endif

    assert(!_pkg_write_queue.empty());
    const size_t buf_count = _pkg_write_queue.size();
    void **bufs = (void**) ::alloca(sizeof(void*) * buf_count + sizeof(size_t) * buf_count);
    size_t *lens = (size_t*) (bufs + buf_count);

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
    if (!_pkg_write_queue.empty())
    {
        launch_write();
        return;
    }

    // 如果本地写队列空了，并且处于关闭流程中，则关闭写通道
    if (_sock_stream.is_reading_shutdown())
        force_close(); // 被动关闭连接
    else if (_closing.load(std::memory_order_relaxed))
        _sock_stream.shutdown_write(); // 主动关闭连接
}

void ProactPackageChannel::handle_io_error(int err)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    NUT_LOG_E(TAG, "loofah error raised, fd %d, error %d: %s", get_socket(),
              err, str_error(err));

    _sock_stream.mark_reading_shutdown();
    _sock_stream.mark_writing_shutdown();
    force_close();
}

}
