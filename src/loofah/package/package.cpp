
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // For ::memcpy()
#include <algorithm> // For std::min()

#include <nut/platform/endian.h>

#include "package.h"


#undef min

#define VALIDATE_MEMBERS() \
    assert((nullptr == _buffer && 0 == _capacity && sizeof(header_type) == _read_index && sizeof(header_type) == _write_index) || \
           (nullptr != _buffer && _read_index <= _write_index && _write_index <= _capacity))

namespace loofah
{

Package::Package(size_t init_cap) noexcept
{
    _buffer = (uint8_t*) ::malloc(sizeof(header_type) + init_cap);
    _capacity = sizeof(header_type) + init_cap;
}

Package::Package(const void *buf, size_t len) noexcept
{
    _buffer = (uint8_t*) ::malloc(sizeof(header_type) + len);
    ::memcpy(_buffer + sizeof(header_type), buf, len);
    _capacity = sizeof(header_type) + len;
    _write_index = sizeof(header_type) + len;
}

Package::~Package() noexcept
{
    clear();
}

bool Package::is_little_endian() const noexcept
{
    return _little_endian;
}

void Package::set_little_endian(bool le) noexcept
{
    _little_endian = le;
}

void Package::clear() noexcept
{
    if (nullptr != _buffer)
        ::free(_buffer);
    _buffer = nullptr;
    _capacity = 0;
    _read_index = sizeof(header_type);
    _write_index = sizeof(header_type);
}

size_t Package::readable_size() const noexcept
{
    VALIDATE_MEMBERS();
    return _write_index - _read_index;
}

const void* Package::readable_data() const noexcept
{
    VALIDATE_MEMBERS();
    return _buffer + _read_index;
}

void Package::skip_read(size_t len) noexcept
{
    assert(len <= readable_size());
    _read_index += len;
}

size_t Package::read(void *buf, size_t len) noexcept
{
    assert(nullptr != buf);
    size_t ret = std::min(len, readable_size());
    ::memcpy(buf, _buffer + _read_index, ret);
    _read_index += ret;
    return ret;
}

size_t Package::writable_size() const noexcept
{
    VALIDATE_MEMBERS();
    if (0 == _capacity)
        return 0;
    return _capacity - _write_index;
}

void* Package::writable_data() noexcept
{
    VALIDATE_MEMBERS();
    return _buffer + _write_index;
}

void Package::skip_write(size_t len) noexcept
{
    assert(len <= writable_size());
    _write_index += len;
}

void Package::ensure_writable_size(size_t write_size) noexcept
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

size_t Package::write(const void *buf, size_t len) noexcept
{
    assert(nullptr != buf);
    ensure_writable_size(len);
    ::memcpy(_buffer + _write_index, buf, len);
    _write_index += len;
    return len;
}

void Package::raw_pack() noexcept
{
    VALIDATE_MEMBERS();
    assert(_read_index >= sizeof(header_type)); // NOTE Space for header

    if (nullptr == _buffer)
    {
        _buffer = (uint8_t*) ::malloc(sizeof(header_type));
        _capacity = sizeof(header_type);
    }

    header_type *pheader = (header_type*)(_buffer + _read_index - sizeof(header_type));
    *pheader = htobe32(readable_size());
    _read_index -= sizeof(header_type);
}

Package::header_type Package::header_betoh(header_type header) noexcept
{
    assert(sizeof(header) == 4);
    return be32toh(header);
}

void Package::raw_rewind() noexcept
{
    VALIDATE_MEMBERS();
    assert(nullptr != _buffer);
    _read_index = 0;
    _write_index = 0;
}

}
