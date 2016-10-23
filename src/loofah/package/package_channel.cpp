
#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>

#include "package_channel.h"


#define READ_BUF_LEN 65536
#define STACK_BUF_COUNT 10

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

void PackageChannel::launch_read()
{
    if (NULL == _read_freg)
        _read_freg = _readed_buffer.new_fregment(READ_BUF_LEN);
    void *buf = _read_freg->buffer;
    _proactor->launch_read(this, &buf, &_read_freg->capacity, 1);
}

void PackageChannel::launch_write()
{
    assert(!_write_queue.empty());

    void *bufs[STACK_BUF_COUNT];
    size_t lens[STACK_BUF_COUNT];
    const size_t buf_count = std::min((size_t) STACK_BUF_COUNT, _write_queue.size());

    queue_t::const_iterator iter = _write_queue.begin();
    for (size_t i = 0; i < buf_count; ++i, ++iter)
    {
        Package *pkg = *iter;
        assert(NULL != pkg);
        bufs[i] = (void*) pkg->readable_data();
        lens[i] = pkg->readable_size();
    }

    _proactor->launch_write(this, bufs, lens, buf_count);
}

void PackageChannel::open(socket_t fd)
{
    ProactChannel::open(fd);

    _proactor->register_handler(this);
    launch_read();
}

void PackageChannel::handle_read_completed(int cb)
{
    if (0 == cb)
    {
        handle_close();
        return;
    }

    // Cache to buffer
    assert(cb <= _read_freg->capacity);
    _read_freg->size = cb;
    _read_freg = _readed_buffer.enqueue_fregment(_read_freg);
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

    nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
    assert(pkg->writable_size() >= pkg_len);
    _readed_buffer.skip_read(sizeof(pkg_len));
    _readed_buffer.read(pkg->writable_data(), pkg_len);
    pkg->skip_write(pkg_len);
    handle_read(pkg);
}

void PackageChannel::handle_write_completed(int cb)
{
    assert(!_write_queue.empty());
    while (cb > 0)
    {
        nut::rc_ptr<Package> pkg = _write_queue.front();
        const size_t readable = pkg->readable_size();
        if (cb >= readable)
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

    if (!_write_queue.empty())
        launch_write();
}

void PackageChannel::write(nut::rc_ptr<Package> pkg)
{
    assert(NULL != pkg);
    _write_queue.push_back(pkg);
    if (1 == _write_queue.size())
        launch_write();
}

}
