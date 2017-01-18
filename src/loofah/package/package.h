
#ifndef ___HEADFILE_C43E4883_DA8E_4E49_A032_276D3E57EE77_
#define ___HEADFILE_C43E4883_DA8E_4E49_A032_276D3E57EE77_

#include "../loofah_config.h"

#include <nut/container/bytestream/input_stream.h>
#include <nut/container/bytestream/output_stream.h>

namespace loofah
{

class LOOFAH_API Package : public nut::InputStream, public nut::OutputStream
{
    NUT_REF_COUNTABLE_OVERRIDE

    const size_t PREPEND_LEN = sizeof(uint32_t);

    void *_buffer = nullptr;
    size_t _capacity = 0;

    size_t _read_index = PREPEND_LEN;
    size_t _write_index = PREPEND_LEN;

    bool _little_endian = false;

private:
    // Non-copyable
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;

public:
    explicit Package(size_t init_cap = 16);
    Package(const void *buf, size_t len);
    ~Package();

    virtual bool is_little_endian() const override;
    virtual void set_little_endian(bool le) override;

    void clear();
    void prepend(const void *header);

    virtual size_t readable_size() const override;
    const void* readable_data() const;
    virtual void skip_read(size_t len) override;
    virtual size_t read(void *buf, size_t len) override;

    size_t writable_size() const;
    void* writable_data();
    void skip_write(size_t len);
    void ensure_writable_size(size_t write_size);
    virtual size_t write(const void *buf, size_t len) override;
};

}

#endif
