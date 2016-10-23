
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
    size_t lens[2];
    const int n = x.readable_pointers(buffers, lens,
                                      buffers + 1, lens + 1);
    for (int i = 0; i < n; ++i)
        write(buffers[i], lens[i]);

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
        assert(NULL != _buffer);
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

size_t RingBuffer::read(void *buf, size_t len)
{
    assert(NULL != buf);
    const size_t readed = look_ahead(buf, len);
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

size_t RingBuffer::look_ahead(void *buf, size_t len) const
{
    assert(NULL != buf);
    const size_t readed = std::min(len, readable_size()),
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

size_t RingBuffer::skip_read(size_t len)
{
    const size_t skiped = std::min(len, readable_size());
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

size_t RingBuffer::readable_pointers(const void **buf_ptr1, size_t *len_ptr1,
                                     const void **buf_ptr2, size_t *len_ptr2) const
{
    if (_write_index == _read_index)
        return 0;

    if (_write_index > _read_index)
    {
        if (NULL != buf_ptr1)
            *buf_ptr1 = (const uint8_t*) _buffer + _read_index;
        if (NULL != len_ptr1)
            *len_ptr1 = _write_index - _read_index;
        return 1;
    }

    if (NULL != buf_ptr1)
        *buf_ptr1 = (const uint8_t*) _buffer + _read_index;
    if (NULL != len_ptr1)
        *len_ptr1 = _capacity - _read_index;
    if (0 == _write_index)
        return 1;

    if (NULL != buf_ptr2)
        *buf_ptr2 = _buffer;
    if (NULL != len_ptr2)
        *len_ptr2 = _write_index;
    return 2;
}

size_t RingBuffer::writable_size() const
{
    assert(_read_index < _capacity && _write_index < _capacity);

    if (_write_index >= _read_index)
        return _capacity - _write_index + _read_index - 1;
    return _read_index - 1 - _write_index;
}

void RingBuffer::write(const void *buf, size_t len)
{
    assert(NULL != buf);
    ensure_writable_size(len);
    const size_t trunk_sz = _capacity - _write_index;
    if (trunk_sz >= len)
    {
        ::memcpy((uint8_t*) _buffer + _write_index, buf, len);
    }
    else
    {
        ::memcpy((uint8_t*) _buffer + _write_index, buf, trunk_sz);
        ::memcpy(_buffer, (const uint8_t*) buf + trunk_sz, len - trunk_sz);
    }
    _write_index += len;
    if (0 != _capacity)
        _write_index %= _capacity;
}

size_t RingBuffer::writable_pointers(void **buf_ptr1, size_t *len_ptr1,
                                  void **buf_ptr2, size_t *len_ptr2)
{
    if (writable_size() == 0)
        return 0;

    if (_write_index < _read_index)
    {
        if (NULL != buf_ptr1)
            *buf_ptr1 = (uint8_t*) _buffer + _write_index;
        if (NULL != len_ptr1)
            *len_ptr1 = _read_index - 1 - _write_index;
        return 1;
    }

    if (NULL != buf_ptr1)
        *buf_ptr1 = (uint8_t*) _buffer + _write_index;
    if (NULL != len_ptr1)
    {
        if (0 == _read_index)
            *len_ptr1 = _capacity - _write_index - 1;
        else
            *len_ptr1 = _capacity - _write_index;
    }
    if (_read_index < 2)
        return 1;

    if (NULL != buf_ptr2)
        *buf_ptr2 = _buffer;
    if (NULL != len_ptr2)
        *len_ptr2 = _read_index - 1;
    return 2;
}

}
