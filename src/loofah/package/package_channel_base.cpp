﻿
#include "../loofah_config.h"

#include <nut/rc/rc_new.h>
#include <nut/logging/logger.h>
#include <nut/platform/endian.h>

#include "package_channel_base.h"
#include "../inet_base/error.h"


#define TAG "loofah.package.package_channel_base"

namespace loofah
{

PackageChannelBase::~PackageChannelBase()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    cancel_force_close_timer();
}

void PackageChannelBase::set_time_wheel(nut::TimeWheel *time_wheel)
{
    assert(nullptr != time_wheel);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(nullptr == _time_wheel);
    _time_wheel = time_wheel;
}

nut::TimeWheel* PackageChannelBase::get_time_wheel() const
{
    return _time_wheel;
}

void PackageChannelBase::set_max_payload_size(size_t max_size)
{
    _max_payload_size = max_size;
}

size_t PackageChannelBase::get_max_payload_size() const
{
    return _max_payload_size;
}

void PackageChannelBase::close_later(bool discard_write)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    // 设置关闭标记
    _closing = true;

    assert(nullptr != _actor);
    if (_actor->is_in_loop_thread())
    {
        // Synchronize
        close(discard_write);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<PackageChannelBase> ref_this(this);
        _actor->run_later([=] { ref_this->close(discard_write); });
    }
}

void PackageChannelBase::setup_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID != _force_close_timer ||
        LOOFAH_FORCE_CLOSE_DELAY <= 0)
        return;

    _force_close_timer = _time_wheel->add_timer(
        LOOFAH_FORCE_CLOSE_DELAY, 0,
        [=] (nut::TimeWheel::timer_id_type id, uint64_t expires) {
            _force_close_timer = NUT_INVALID_TIMER_ID;
            force_close();
        });
}

void PackageChannelBase::cancel_force_close_timer()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    if (nullptr == _time_wheel || NUT_INVALID_TIMER_ID == _force_close_timer)
        return;
    _time_wheel->cancel_timer(_force_close_timer);
    _force_close_timer = NUT_INVALID_TIMER_ID;
}

void PackageChannelBase::split_and_handle_packages(size_t extra_readed)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    if (_closing || get_sock_stream().is_reading_shutdown())
        return;

    // 分包
    nut::rc_ptr<Package> buffer_pkg = _reading_pkg;
    std::vector<nut::rc_ptr<Package>> full_packages;
    const size_t total_size = buffer_pkg->readable_size() + extra_readed;
    size_t processed_size = 0;
    bool payload_oversize = false;
    while (processed_size < total_size)
    {
        const size_t remained_size = total_size - processed_size;
        const uint8_t *readable_data = ((const uint8_t*) buffer_pkg->readable_data()) + processed_size;

        // header 尚未读完
        if (remained_size < sizeof(Package::header_type))
        {
            if (0 == processed_size)
            {
                buffer_pkg->skip_write(extra_readed);
            }
            else
            {
                nut::rc_ptr<Package> new_pkg = nut::rc_new<Package>(LOOFAH_INIT_READ_PKG_SIZE);
                new_pkg->raw_rewind();
                ::memcpy(new_pkg->writable_data(), readable_data, remained_size);
                new_pkg->skip_write(remained_size);

                _reading_pkg = std::move(new_pkg);
            }
            break;
        }

        // 检查 payload 大小
        Package::header_type payload_size = *(const Package::header_type*) readable_data;
        payload_size = be32toh(payload_size);
        if (payload_size > _max_payload_size)
        {
            payload_oversize = true;
            break;
        }

        // payload 尚未读完
        const size_t pkg_size = sizeof(Package::header_type) + payload_size;
        if (remained_size < pkg_size)
        {
            if (0 == processed_size)
            {
                buffer_pkg->skip_write(extra_readed);

                // NOTE The last extra header_type space is for reading next package size
                assert(pkg_size - total_size > 0);
                buffer_pkg->ensure_writable_size(pkg_size - total_size + sizeof(Package::header_type));
            }
            else
            {
                // NOTE The last extra header_type space is for reading next package size
                nut::rc_ptr<Package> new_pkg = nut::rc_new<Package>(payload_size + sizeof(Package::header_type));
                new_pkg->raw_rewind();
                ::memcpy(new_pkg->writable_data(), readable_data, remained_size);
                new_pkg->skip_write(remained_size);

                _reading_pkg = std::move(new_pkg);
            }
            break;
        }

        // 完整 package
        if (0 == processed_size)
        {
            buffer_pkg->skip_write(pkg_size - buffer_pkg->readable_size());
            full_packages.push_back(buffer_pkg);
        }
        else
        {
            nut::rc_ptr<Package> new_pkg = nut::rc_new<Package>(payload_size);
            new_pkg->raw_rewind();
            ::memcpy(new_pkg->writable_data(), readable_data, pkg_size);
            new_pkg->skip_write(pkg_size);

            full_packages.push_back(std::move(new_pkg));
        }
        _reading_pkg = nullptr;
        processed_size += pkg_size;
    }

    // handle
    for (size_t i = 0, sz = full_packages.size(); i < sz; ++i)
    {
        Package *pkg = full_packages.at(i);

        assert(pkg->readable_size() >= sizeof(Package::header_type));
        Package::header_type payload_size = *(const Package::header_type*) pkg->readable_data();
        payload_size = be32toh(payload_size);
        assert(pkg->readable_size() == sizeof(Package::header_type) + payload_size);

        pkg->skip_read(sizeof(Package::header_type));
        handle_read(pkg);
        if (_closing || get_sock_stream().is_reading_shutdown())
            break;
    }
    if (payload_oversize && !_closing && !get_sock_stream().is_reading_shutdown())
        handle_error(LOOFAH_ERR_PKG_OVERSIZE);
}

void PackageChannelBase::handle_reading_shutdown()
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    close();
}

void PackageChannelBase::handle_error(int err)
{
    NUT_DEBUGGING_ASSERT_ALIVE;
    assert(nullptr != _actor && _actor->is_in_loop_thread());

    NUT_LOG_E(TAG, "loofah error raised %d: %s", err, str_error(err));
    close();
}

void PackageChannelBase::write_later(Package *pkg)
{
    assert(nullptr != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (_closing || get_sock_stream().is_writing_shutdown())
    {
        const socket_t fd = get_sock_stream().get_socket();
        if (_closing)
            NUT_LOG_E(TAG, "channel is closing, writing package discard. fd %d", fd);
        else
            NUT_LOG_E(TAG, "write channel is closed, writing package discard. fd %d", fd);
        return;
    }

    assert(nullptr != _actor);
    if (_actor->is_in_loop_thread())
    {
        // Synchronize
        write(pkg);
    }
    else
    {
        // Asynchronize
        nut::rc_ptr<PackageChannelBase> ref_this(this);
        nut::rc_ptr<Package> ref_pkg(pkg);
        _actor->run_later([=] { ref_this->write(ref_pkg); });
    }
}

}
