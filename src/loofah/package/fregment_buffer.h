
#ifndef ___HEADFILE_094F11D2_4F74_47A1_A6E9_2F453DDFC214_
#define ___HEADFILE_094F11D2_4F74_47A1_A6E9_2F453DDFC214_

#include <stdint.h>
#include <string.h> // for size_t

namespace loofah
{

class FregmentBuffer
{
public:
    struct Fregment
    {
        size_t capacity = 0;
        size_t size = 0;
        struct Fregment *next = NULL;

        uint8_t buffer[1];
    };

private:
    struct Fregment *_read_fregment = NULL;
    struct Fregment *_write_fregment = NULL;

    size_t _read_index = 0;
    size_t _read_available = 0;

    void enqueue(Fregment *freg);

public:
    FregmentBuffer();
    FregmentBuffer(const FregmentBuffer& x);
    ~FregmentBuffer();
    FregmentBuffer& operator=(const FregmentBuffer& x);

    void clear();

    size_t readable_size() const;
    size_t read(void *buf, size_t size);
    size_t look_ahead(void *buf, size_t size) const;
    size_t skip_read(size_t size);

    /**
     * @return 返回指针个数
     */
    int readable_pointers(const void **bufs, size_t buf_ptr_strike,
                          size_t *sizes, size_t size_ptr_strike,
                          size_t ptr_count) const;

    void write(const void *buf, size_t size);

    Fregment* new_fregment(size_t size);
    void delete_fregment(Fregment *freg);

    /**
     * @return 如果内存片段被征用，则返回 NULL，否则返回传入的同一指针
     */
    Fregment* enqueue_fregment(Fregment *freg);
};

}

#endif
