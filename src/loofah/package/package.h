
#ifndef ___HEADFILE_C43E4883_DA8E_4E49_A032_276D3E57EE77_
#define ___HEADFILE_C43E4883_DA8E_4E49_A032_276D3E57EE77_

#include <nut/rc/rc_ptr.h>

namespace loofah
{

class Package
{
    NUT_REF_COUNTABLE

    void *_buffer = NULL;
    size_t _capacity = 0;

    size_t _read_index = 0;
    size_t _write_index = 0;

public:
    explicit Package(size_t init_cap);
    Package(const void *buf, size_t size);
    ~Package();

    void clear();

    size_t readable_size() const;
    const void* readable_data() const;
    void skip_read(size_t size);

    size_t writable_size() const;
    void* writable_data();
    void skip_write(size_t size);
    void ensure_writable_size(size_t write_size);
    void write(const void *buf, size_t size);
};

}

#endif
