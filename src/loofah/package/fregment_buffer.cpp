
#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#include "fregment_buffer.h"


namespace loofah
{

FregmentBuffer::FregmentBuffer()
{}

FregmentBuffer::FregmentBuffer(const FregmentBuffer& x)
{
    *this = x;
}

FregmentBuffer::~FregmentBuffer()
{
    clear();
}

FregmentBuffer& FregmentBuffer::operator=(const FregmentBuffer& x)
{
    clear();

    Fregment *p = x._read_fregment;
    while (NULL != p)
    {
        Fregment *new_freg = new_fregment(p->size);
        assert(NULL != new_freg);
        ::memcpy(new_freg->buffer, p->buffer, p->size);
        new_freg->size = p->size;

        enqueue(new_freg);
    }
    _read_index = x._read_index;
    _read_available = x._read_available;

    return *this;
}

void FregmentBuffer::enqueue(Fregment *freg)
{
    assert(NULL != freg && freg->capacity >= freg->size);

    freg->next = NULL;
    if (NULL == _write_fregment)
    {
        _read_fregment = freg;
        _write_fregment = freg;
        _read_index = 0;
    }
    else
    {
        _write_fregment->next = freg;
        _write_fregment = freg;
    }
    _read_available += freg->size;
}

void FregmentBuffer::clear()
{
    Fregment *p = _read_fregment;
    while (NULL != p)
    {
        Fregment *next = p->next;
        delete_fregment(p);
        p = next;
    }
    _read_fregment = NULL;
    _write_fregment = NULL;
    _read_index = 0;
    _read_available = 0;
}

size_t FregmentBuffer::readable_size() const
{
    return _read_available;
}

size_t FregmentBuffer::read(void *buf, size_t size)
{
    const size_t can_read = std::min(size, _read_available);
    size_t readed = 0;
    while (readed < can_read)
    {
        assert(NULL != _read_fregment && _read_fregment->size >= _read_index);
        const size_t can_read_once = std::min(_read_fregment->size - _read_index,
                                              can_read - readed);
        const bool full_read = (_read_fregment->size - _read_index <=
                                can_read - readed);
        ::memcpy((uint8_t*) buf + readed, _read_fregment->buffer + _read_index,
                 can_read_once);
        if (full_read)
        {
            Fregment *next = _read_fregment->next;
            delete_fregment(_read_fregment);
            _read_fregment = next;
            if (NULL == next)
                _write_fregment = NULL;
            _read_index = 0;
        }
        else
        {
            _read_index += can_read_once;
        }
        readed += can_read_once;
    }
    assert(readed == can_read);
    _read_available -= can_read;
    return can_read;
}

size_t FregmentBuffer::look_ahead(void *buf, size_t size) const
{
    const size_t can_read = std::min(size, _read_available);
    size_t readed = 0;
    Fregment *p = _read_fregment;
    size_t read_index = _read_index;
    while (readed < can_read)
    {
        assert(NULL != p && p->size >= read_index);
        const size_t can_read_once = std::min(p->size - read_index,
                                              can_read - readed);
        const bool full_read = (p->size - read_index <= can_read - readed);
        ::memcpy((uint8_t*) buf + readed, p->buffer + read_index,
                 can_read_once);
        if (full_read)
        {
            p = p->next;
            read_index = 0;
        }
        readed += can_read_once;
    }
    assert(readed == can_read);
    return can_read;
}

size_t FregmentBuffer::skip_read(size_t size)
{
    size_t can_skip = std::min(size, _read_available);
    size_t skiped = 0;
    while (skiped < can_skip)
    {
        assert(NULL != _read_fregment && _read_fregment->size >= _read_index);
        const size_t can_skip_once = std::min(_read_fregment->size - _read_index,
                                              can_skip - skiped);
        const bool full_skip = (_read_fregment->size - _read_index <=
                                can_skip - skiped);
        if (full_skip)
        {
            Fregment *next = _read_fregment->next;
            delete_fregment(_read_fregment);
            _read_fregment = next;
            if (NULL == next)
                _write_fregment = NULL;
            _read_index = 0;
        }
        else
        {
            _read_index += can_skip_once;
        }
        skiped += can_skip_once;
    }
    assert(skiped == can_skip);
    _read_available -= can_skip;
    return can_skip;
}

int FregmentBuffer::readable_pointers(const void **bufs, size_t buf_ptr_strike,
                                      size_t *sizes, size_t size_ptr_strike,
                                      size_t ptr_count) const
{
    assert(NULL != bufs && NULL != sizes);
    if (0 == buf_ptr_strike)
        buf_ptr_strike = sizeof(const void*);
    if (0 == size_ptr_strike)
        size_ptr_strike = sizeof(size_t);

    int rs = 0;
    Fregment *p = _read_fregment;
    size_t read_index = _read_index;
    while (NULL != p && rs < (int) ptr_count)
    {
        assert(p->size >= read_index);
        *bufs = p->buffer + read_index;
        *sizes = p->size - read_index;

        bufs = (const void**) (reinterpret_cast<long>(bufs) + buf_ptr_strike);
        sizes = (size_t *) (reinterpret_cast<long>(sizes) + size_ptr_strike);

        p = p->next;
        read_index = 0;
        ++rs;
    }
    return rs;
}

void FregmentBuffer::write(const void *buf, size_t size)
{
    assert(NULL != buf);

    if (NULL != _write_fregment &&
        _write_fregment->capacity - _write_fregment->size >= size)
    {
        ::memcpy(_write_fregment->buffer + _write_fregment->size, buf, size);
        _write_fregment->size += size;
        _read_available += size;
        return;
    }

    Fregment *freg = new_fregment(size);
    ::memcpy(freg->buffer, buf, size);
    freg->size = size;
    enqueue(freg);
}

FregmentBuffer::Fregment* FregmentBuffer::new_fregment(size_t size)
{
    assert(size > 0);
    Fregment* p = (Fregment*) ::malloc(sizeof(Fregment) + size - 1);
    assert(NULL != p);
    p->capacity = size;
    p->size = 0;
    p->next = NULL;
    return p;
}

void FregmentBuffer::delete_fregment(Fregment *freg)
{
    assert(NULL != freg);
    ::free(freg);
}

FregmentBuffer::Fregment* FregmentBuffer::enqueue_fregment(Fregment *freg)
{
    assert(NULL != freg);
    if (NULL != _write_fregment &&
        _write_fregment->capacity - _write_fregment->size >= freg->size)
    {
        ::memcpy(_write_fregment->buffer + _write_fregment->size, freg->buffer,
                 freg->size);
        _write_fregment->size += freg->size;
        _read_available += freg->size;
        freg->size = 0;
        return freg;
    }

    enqueue(freg);
    return NULL;
}

}
