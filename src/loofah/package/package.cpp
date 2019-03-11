
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // For ::memcpy()
#include <algorithm> // For std::min()

#include <nut/platform/endian.h>

#include "package.h"


#define VALIDATE_MEMBERS() \
    assert((nullptr == _buffer && 0 == _capacity && sizeof(header_type) == _read_index && sizeof(header_type) == _write_index) || \
           (nullptr != _buffer && _read_index <= _write_index && _write_index <= _capacity))

namespace loofah
{

Package::Package(size_t init_cap)
{
    _buffer = (uint8_t*) ::malloc(sizeof(header_type) + init_cap);
    _capacity = sizeof(header_type) + init_cap;
}

Package::Package(const void *buf, size_t len)
{
    _buffer = (uint8_t*) ::malloc(sizeof(header_type) + len);
    ::memcpy(_buffer + sizeof(header_type), buf, len);
    _capacity = sizeof(header_type) + len;
    _write_index = sizeof(header_type) + len;
}

Package::~Package()
{
    clear();
}

bool Package::is_little_endian() const
{
    return _little_endian;
}

void Package::set_little_endian(bool le)
{
    _little_endian = le;
}

void Package::clear()
{
    if (nullptr != _buffer)
        ::free(_buffer);
    _buffer = nullptr;
    _capacity = 0;
    _read_index = sizeof(header_type);
    _write_index = sizeof(header_type);
}

void Package::pack()
{
    VALIDATE_MEMBERS();
    assert(_read_index >= sizeof(header_type)); // NOTE Space for header

    if (nullptr == _buffer)
    {
        _buffer = (uint8_t*) ::malloc(sizeof(header_type));
        _capacity = sizeof(header_type);
    }

    uint32_t *pheader = (uint32_t*)(_buffer + _read_index - sizeof(header_type));
    *pheader = htobe32(readable_size());
    _read_index -= sizeof(header_type);
}

size_t Package::readable_size() const
{
    VALIDATE_MEMBERS();
    return _write_index - _read_index;
}

const void* Package::readable_data() const
{
    VALIDATE_MEMBERS();
    return _buffer + _read_index;
}

void Package::skip_read(size_t len)
{
    assert(len <= readable_size());
    _read_index += len;
}

size_t Package::read(void *buf, size_t len)
{
    assert(nullptr != buf);
    size_t ret = (std::min)(len, readable_size());
    ::memcpy(buf, _buffer + _read_index, ret);
    _read_index += ret;
    return ret;
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
    return _buffer + _write_index;
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

    const size_t data_size = _write_index - _read_index;
    const size_t min_cap = sizeof(header_type) + data_size + write_size;
    if (_capacity >= min_cap)
    {
        assert(nullptr != _buffer);
        ::memmove(_buffer + sizeof(header_type), _buffer + _read_index, data_size);
        _read_index = sizeof(header_type);
        _write_index = sizeof(header_type) + data_size;
        return;
    }

    size_t new_cap = sizeof(header_type) + data_size * 3 / 2;
    if (new_cap < min_cap)
        new_cap = min_cap;

    if (_read_index <= sizeof(header_type))
    {
        _buffer = (uint8_t*) ::realloc(_buffer, new_cap);
        _capacity = new_cap;
    }
    else
    {
        uint8_t *new_buf = (uint8_t*) ::malloc(new_cap);
        assert(nullptr != new_buf && nullptr != _buffer);
        ::memcpy(new_buf + sizeof(header_type), _buffer + _read_index, data_size);
        ::free(_buffer);
        _buffer = new_buf;
        _capacity = new_cap;
        _read_index = sizeof(header_type);
        _write_index = sizeof(header_type) + data_size;
    }
}

size_t Package::write(const void *buf, size_t len)
{
    assert(nullptr != buf);
    ensure_writable_size(len);
    ::memcpy(_buffer + _write_index, buf, len);
    _write_index += len;
    return len;
}

}
