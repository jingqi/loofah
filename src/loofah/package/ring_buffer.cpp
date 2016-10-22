
#include <assert.h>
#include <stdlib.h>
#include <algorithm> // for std::min()

#include "ring_buffer.h"

namespace loofah
{

RingBuffer::RingBuffer()
{}

RingBuffer::RingBuffer(const RingBuffer& x)
{
    *this = x;
}

RingBuffer::~RingBuffer()
{
    if (NULL != _buffer)
        ::free(_buffer);
    _buffer = NULL;
    _capacity = 0;
    _read_index = 0;
    _write_index = 0;
}

RingBuffer& RingBuffer::operator=(const RingBuffer& x)
{
    clear();
    ensure_writable_size(x.readable_size());

    const void *buffers[2];
    size_t sizes[2];
    const int n = x.readable_pointers(buffers, sizes,
                                      buffers + 1, sizes + 1);
    for (int i = 0; i < n; ++i)
        write(buffers[i], sizes[i]);

    return *this;
}

void RingBuffer::ensure_writable_size(size_t write_size)
{
    if (writable_size() >= write_size)
        return;

    const size_t rd_sz = readable_size();
    size_t new_cap = _capacity * 3 / 2;
    if (new_cap < rd_sz + write_size + 1)
        new_cap = rd_sz + write_size + 1;

    if (_write_index >= _read_index)
    {
        assert(rd_sz == _write_index - _read_index);
        _buffer = ::realloc(_buffer, new_cap);
        assert(NULL != buffer);
        _capacity = new_cap;
    }
    else
    {
        assert(rd_sz == _capacity - _read_index + _write_index);
        void *new_buffer = ::malloc(new_cap);
        assert(NULL != new_buffer);
        const size_t trunk_sz = _capacity - _read_index;
        ::memcpy(new_buffer, (const uint8_t*) _buffer + _read_index, trunk_sz);
        ::memcpy((uint8_t*) new_buffer + trunk_sz, _buffer, _write_index);
        ::free(_buffer);
        _buffer = new_buffer;
        _capacity = new_cap;
        _read_index = 0;
        _write_index = rd_sz;
    }
}

void RingBuffer::clear()
{
    _read_index = 0;
    _write_index = 0;
}

size_t RingBuffer::readable_size() const
{
    assert(_read_index < _capacity && _write_index < _capacity);

    if (_write_index >= _read_index)
        return _write_index - _read_index;
    return _capacity - _read_index + _write_index;
}

size_t RingBuffer::read(void *buf, size_t size)
{
    assert(NULL != buf);
    const size_t readed = look_ahead(buf, size);
    _read_index += readed;
    if (0 != _capacity)
        _read_index %= _capacity;

    // Reset to zero, this does no harm
    if (_read_index == _write_index)
    {
        _read_index = 0;
        _write_index = 0;
    }
    return readed;
}

size_t RingBuffer::look_ahead(void *buf, size_t size) const
{
    assert(NULL != buf);
    const size_t readed = std::min(size, readable_size()),
        trunk_sz = _capacity - _read_index;
    if (trunk_sz >= readed)
    {
        ::memcpy(buf, (const uint8_t*) _buffer + _read_index, readed);
    }
    else
    {
        ::memcpy(buf, (const uint8_t*) _buffer + _read_index, trunk_sz);
        ::memcpy((uint8_t*) buf + trunk_sz, _buffer, readed - trunk_sz);
    }
    return readed;
}

size_t RingBuffer::skip_read(size_t size)
{
    const size_t skiped = std::min(size, readable_size());
    _read_index += skiped;
    if (0 != _capacity)
        _read_index %= _capacity;

    // Reset to zero, this does no harm
    if (_read_index == _write_index)
    {
        _read_index = 0;
        _write_index = 0;
    }
    return skiped;
}

int RingBuffer::readable_pointers(const void **buf1, size_t *size1,
                                  const void **buf2, size_t *size2) const
{
    if (_write_index == _read_index)
        return 0;

    if (_write_index > _read_index)
    {
        if (NULL != buf1)
            *buf1 = (const uint8_t*) _buffer + _read_index;
        if (NULL != size1)
            *size1 = _write_index - _read_index;
        return 1;
    }

    if (NULL != buf1)
        *buf1 = (const uint8_t*) _buffer + _read_index;
    if (NULL != size1)
        *size1 = _capacity - _read_index;
    if (0 == _write_index)
        return 1;

    if (NULL != buf2)
        *buf2 = _buffer;
    if (NULL != size2)
        *size2 = _write_index;
    return 2;
}

size_t RingBuffer::writable_size() const
{
    assert(_read_index < _capacity && _write_index < _capacity);

    if (_write_index >= _read_index)
        return _capacity - _write_index + _read_index - 1;
    return _read_index - 1 - _write_index;
}

void RingBuffer::write(const void *buf, size_t size)
{
    assert(NULL != buf);
    ensure_writable_size(size);
    const size_t trunk_sz = _capacity - _write_index;
    if (trunk_sz >= size)
    {
        ::memcpy((uint8_t*) _buffer + _write_index, buf, size);
    }
    else
    {
        ::memcpy((uint8_t*) _buffer + _write_index, buf, trunk_sz);
        ::memcpy(_buffer, (const uint8_t*) buf + trunk_sz, size - trunk_sz);
    }
    _write_index += size;
    if (0 != _capacity)
        _write_index %= _capacity;
}

int RingBuffer::writable_pointers(void **buf1, size_t *size1,
                                  void **buf2, size_t *size2)
{
    if (writable_size() == 0)
        return 0;

    if (_write_index < _read_index)
    {
        if (NULL != buf1)
            *buf1 = (uint8_t*) _buffer + _write_index;
        if (NULL != size1)
            *size1 = _read_index - 1 - _write_index;
        return 1;
    }

    if (NULL != buf1)
        *buf1 = (uint8_t*) _buffer + _write_index;
    if (NULL != size1)
    {
        if (0 == _read_index)
            *size1 = _capacity - _write_index - 1;
        else
            *size1 = _capacity - _write_index;
    }
    if (_read_index < 2)
        return 1;

    if (NULL != buf2)
        *buf2 = _buffer;
    if (NULL != size2)
        *size2 = _read_index - 1;
    return 2;
}

}
