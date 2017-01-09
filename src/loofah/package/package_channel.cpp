
#include <assert.h>
#include <algorithm> // for std::min()

#include <nut/rc/rc_new.h>

#include "package_channel.h"


#define READ_BUF_LEN 65536
#define STACK_BUF_COUNT 10

namespace loofah
{

PackageChannel::~PackageChannel()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (NULL != _read_frag)
        nut::FragmentBuffer::delete_fragment(_read_frag);
    _read_frag = NULL;
}

void PackageChannel::set_proactor(Proactor *proactor)
{
    assert(NULL != proactor && NULL == _proactor);
    NUT_DEBUGGING_ASSERT_ALIVE;

    _proactor = proactor;
}

void PackageChannel::launch_read()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (NULL == _read_frag)
        _read_frag = nut::FragmentBuffer::new_fragment(READ_BUF_LEN);
    void *buf = _read_frag->buffer;
    assert(NULL != _proactor);
    _proactor->async_launch_read(this, &buf, &_read_frag->capacity, 1);
}

void PackageChannel::launch_write()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(!_write_queue.empty());
    void *bufs[STACK_BUF_COUNT];
    size_t lens[STACK_BUF_COUNT];
    const size_t buf_count = (std::min)((size_t) STACK_BUF_COUNT, _write_queue.size());

    queue_t::const_iterator iter = _write_queue.begin();
    for (size_t i = 0; i < buf_count; ++i, ++iter)
    {
        Package *pkg = *iter;
        assert(NULL != pkg);
        bufs[i] = (void*) pkg->readable_data();
        lens[i] = pkg->readable_size();
    }

    assert(NULL != _proactor);
    _proactor->async_launch_write(this, bufs, lens, buf_count);
}

void PackageChannel::write(Package *pkg)
{
    assert(NULL != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    uint32_t header = pkg->readable_size();
    pkg->prepend(&header);
    _write_queue.push_back(pkg);
    if (1 == _write_queue.size())
        launch_write();
}

void PackageChannel::close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(NULL != _proactor);
    _proactor->async_unregister_handler(this);
    _sock_stream.close();
    handle_close(); // NOTE 这里可能会导致自身被删除，不能再做任何操作了
}

void PackageChannel::open(socket_t fd)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    ProactChannel::open(fd);

    assert(NULL != _proactor);
    _proactor->async_register_handler(this);
    launch_read();
}

void PackageChannel::handle_read_completed(int cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    if (0 == cb)
    {
        assert(NULL != _proactor);
        _proactor->async_unregister_handler(dynamic_cast<ProactHandler*>(this));
        _sock_stream.close();
        handle_close(); // NOTE 这里可能会导致自身被删除，不能再做任何操作了
        return;
    }

    // Cache to buffer
    assert(cb <= _read_frag->capacity);
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

    nut::rc_ptr<Package> pkg = nut::rc_new<Package>(pkg_len);
    assert(pkg->writable_size() >= pkg_len);
    _readed_buffer.skip_read(sizeof(pkg_len));
    _readed_buffer.read(pkg->writable_data(), pkg_len);
    pkg->skip_write(pkg_len);
    handle_read(pkg);
}

void PackageChannel::handle_write_completed(int cb)
{
    NUT_DEBUGGING_ASSERT_ALIVE;

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

void PackageChannel::async_write(Package *pkg)
{
    assert(NULL != pkg);
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(NULL != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        write(pkg);
        return;
    }

    class WriteTask : public nut::Runnable
    {
        PackageChannel *_channel;
        nut::rc_ptr<Package> _pkg;

    public:
        WriteTask(PackageChannel *c, Package *p)
            : _channel(c), _pkg(p)
        {}

        virtual void run() override
        {
            _channel->write(_pkg);
        }
    };
    _proactor->run_in_loop_thread(nut::rc_new<WriteTask>(this, pkg));
}

void PackageChannel::async_close()
{
    NUT_DEBUGGING_ASSERT_ALIVE;

    assert(NULL != _proactor);
    if (_proactor->is_in_loop_thread())
    {
        close();
        return;
    }

    class CloseTask : public nut::Runnable
    {
        PackageChannel *_channel;

    public:
        CloseTask(PackageChannel *c)
            : _channel(c)
        {}

        virtual void run() override
        {
            _channel->close();
        }
    };
    _proactor->run_in_loop_thread(nut::rc_new<CloseTask>(this));
}

}
