
#include <assert.h>
#include <stdlib.h>
#include <string.h> // for ::memcpy()

#include "package.h"


namespace loofah
{

Package::Package(size_t init_cap)
{
    _buffer = ::malloc(init_cap);
    _capacity = init_cap;
}

Package::Package(const void *buf, size_t size)
{
    _buffer = ::malloc(size);
    ::memcpy(_buffer, buf, size);
    _capacity = size;
    _write_index = size;
}

Package::~Package()
{
    clear();
}

void Package::clear()
{
    if (NULL != _buffer)
        ::free(_buffer);
    _buffer = NULL;
    _capacity = 0;
    _read_index = 0;
    _write_index = 0;
}

size_t Package::readable_size() const
{
    assert(_read_index <= _write_index && _write_index <= _capacity);
    return _write_index - _read_index;
}

const void* Package::readable_data() const
{
    return (const uint8_t*) _buffer + _read_index;
}

void Package::skip_read(size_t size)
{
    assert(size <= readable_size());
    _read_index += size;
}

size_t Package::writable_size() const
{
    assert(_read_index <= _write_index && _write_index <= _capacity);
    return _capacity - _write_index;
}

void* Package::writable_data()
{
    return (uint8_t*) _buffer + _write_index;
}

void Package::skip_write(size_t size)
{
    assert(size <= writable_size());
    _write_index += size;
}

void Package::ensure_writable_size(size_t write_size)
{
    assert(_read_index <= _write_index && _write_index <= _capacity);
    if (_capacity - _write_index >= write_size)
        return;

    const size_t data_sz = _write_index - _read_index;
    size_t new_cap = data_sz * 3 / 2;
    if (new_cap < data_sz + write_size)
        new_cap = data_sz + write_size;

    if (0 == _read_index)
    {
        _buffer = ::realloc(_buffer, new_cap);
        _capacity = new_cap;
        return;
    }
    else
    {
        void *new_buf = ::malloc(new_cap);
        assert(NULL != new_buf);
        if (NULL != _buffer)
        {
            ::memcpy(new_buf, (const uint8_t*) _buffer + _read_index, data_sz);
            ::free(_buffer);
        }
        _buffer = new_buf;
        _capacity = new_cap;
        _read_index = 0;
        _write_index = data_sz;
    }
}

void Package::write(const void *buf, size_t size)
{
    assert(NULL != buf);
    ensure_writable_size(size);
    ::memcpy((uint8_t*) _buffer + _write_index, buf, size);
    _write_index += size;
}

}
