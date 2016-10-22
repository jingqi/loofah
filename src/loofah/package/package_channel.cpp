
#include <assert.h>

#include <nut/rc/rc_new.h>

#include "package_channel.h"


#define READ_BUF_LEN 65536

namespace loofah
{

PackageChannel::PackageChannel(Proactor *proactor)
    : _proactor(proactor)
{
    assert(NULL != proactor);
}

PackageChannel::~PackageChannel()
{
    if (NULL != _read_freg)
        _readed_buffer.delete_fregment(_read_freg);
    _read_freg = NULL;
}

void PackageChannel::open(socket_t fd)
{
    ProactChannel::open(fd);

    _proactor->register_handler(this);
    if (NULL == _read_freg)
        _read_freg = _readed_buffer.new_fregment(READ_BUF_LEN);
    _proactor->launch_read(this, _read_freg->buffer, _read_freg->capacity);
}

void PackageChannel::handle_read_completed(void *buf, int cb)
{
    assert(NULL != buf);

    if (0 == cb)
    {
        handle_close();
        return;
    }

    // Cache to buffer
    assert(buf == _read_freg->buffer && cb <= _read_freg->capacity);
    _read_freg->size = cb;
    _read_freg = _readed_buffer.enqueue_fregment(_read_freg);
    if (NULL == _read_freg)
        _read_freg = _readed_buffer.new_fregment(READ_BUF_LEN);
    _proactor->launch_read(this, _read_freg->buffer, _read_freg->capacity);

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

    nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
    assert(pkg->writable_size() == pkg_len);
    _readed_buffer.skip_read(sizeof(pkg_len));
    _readed_buffer.read(pkg->writable_data(), pkg_len);
    pkg->skip_write(pkg_len);
    handle_read(pkg);
}

void PackageChannel::handle_write_completed(void *buf, int cb)
{
    assert(NULL != buf && !_write_queue.empty());
    nut::rc_ptr<Package> pkg = _write_queue.front();
    assert(buf == pkg->readable_data() && cb <= pkg->readable_size());
    pkg->skip_read(cb);
    if (0 == pkg->readable_size())
        _write_queue.pop();

    if (!_write_queue.empty())
    {
        pkg = _write_queue.front();
        _proactor->launch_write(this, (void*) pkg->readable_data(), pkg->readable_size());
    }
}

void PackageChannel::write(nut::rc_ptr<Package> pkg)
{
    assert(NULL != pkg);
    _write_queue.push(pkg);
    if (1 == _write_queue.size())
        _proactor->launch_write(this, (void*) pkg->readable_data(), pkg->readable_size());
}

}
