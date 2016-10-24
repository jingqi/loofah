
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // for ::memcpy()

#include "package.h"


#define VALIDATE_MEMBERS() \
    assert((NULL == _buffer && 0 == _capacity && PREPEND_LEN == _read_index && \
            PREPEND_LEN == _write_index) || \
           (NULL != _buffer && _read_index <= _write_index && \
            _write_index <= _capacity))

namespace loofah
{

Package::Package(size_t init_cap)
{
    _buffer = ::malloc(PREPEND_LEN + init_cap);
    _capacity = PREPEND_LEN + init_cap;
}

Package::Package(const void *buf, size_t len)
{
    _buffer = ::malloc(PREPEND_LEN + len);
    ::memcpy((uint8_t*) _buffer + PREPEND_LEN, buf, len);
    _capacity = PREPEND_LEN + len;
    _write_index = PREPEND_LEN + len;
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
    _read_index = PREPEND_LEN;
    _write_index = PREPEND_LEN;
}

void Package::prepend(const void *header)
{
    assert(NULL != header);
    VALIDATE_MEMBERS();
    assert(_read_index >= PREPEND); // NOTE A package can only be prepended once

    if (NULL == _buffer)
    {
        _buffer = ::malloc(PREPEND_LEN);
        _capacity = PREPEND_LEN;
    }
    ::memcpy(_buffer, header, PREPEND_LEN);
    if (_read_index > PREPEND_LEN)
    {
        ::memmove((uint8_t*) _buffer + PREPEND_LEN, (uint8_t*) _buffer + _read_index,
            _write_index - _read_index);
        _write_index -= _read_index - PREPEND_LEN;
    }
    _read_index = 0;
}

size_t Package::readable_size() const
{
    VALIDATE_MEMBERS();
    return _write_index - _read_index;
}

const void* Package::readable_data() const
{
    VALIDATE_MEMBERS();
    return (const uint8_t*) _buffer + _read_index;
}

void Package::skip_read(size_t len)
{
    assert(len <= readable_size());
    _read_index += len;
}

size_t Package::writable_size() const
{
    VALIDATE_MEMBERS();
    if (0 == _capacity)
        return 0;
    return _capacity - _write_index;
}

void* Package::writable_data()
{
    VALIDATE_MEMBERS();
    return (uint8_t*) _buffer + _write_index;
}

void Package::skip_write(size_t len)
{
    assert(len <= writable_size());
    _write_index += len;
}

void Package::ensure_writable_size(size_t write_size)
{
    VALIDATE_MEMBERS();

    if (_capacity >= _write_index + write_size)
        return;

    const size_t data_sz = _write_index - _read_index;
    size_t new_cap = PREPEND_LEN + data_sz * 3 / 2;
    if (new_cap < PREPEND_LEN + data_sz + write_size)
        new_cap = PREPEND_LEN + data_sz + write_size;

    if (_read_index <= PREPEND_LEN)
    {
        _buffer = ::realloc(_buffer, new_cap);
        _capacity = new_cap;
    }
    else
    {
        void *new_buf = ::malloc(new_cap);
        assert(NULL != new_buf);
        if (NULL != _buffer)
        {
            ::memcpy((uint8_t*) new_buf + PREPEND_LEN, (const uint8_t*) _buffer +
                     _read_index, data_sz);
            ::free(_buffer);
        }
        _buffer = new_buf;
        _capacity = new_cap;
        _read_index = PREPEND_LEN;
        _write_index = PREPEND_LEN + data_sz;
    }
}

void Package::write(const void *buf, size_t len)
{
    assert(NULL != buf);
    ensure_writable_size(len);
    ::memcpy((uint8_t*) _buffer + _write_index, buf, len);
    _write_index += len;
}

}
