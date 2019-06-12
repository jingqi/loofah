
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

public:
    typedef uint32_t header_type;

public:
    explicit Package(size_t init_cap = 16) noexcept;
    Package(const void *buf, size_t len) noexcept;
    ~Package() noexcept;

    virtual bool is_little_endian() const noexcept override;
    virtual void set_little_endian(bool le) noexcept override;

    void clear() noexcept;

    virtual size_t readable_size() const noexcept override;
    const void* readable_data() const noexcept;
    virtual void skip_read(size_t len) noexcept override;
    virtual size_t read(void *buf, size_t len) noexcept override;

    size_t writable_size() const noexcept;
    void* writable_data() noexcept;
    void skip_write(size_t len) noexcept;
    void ensure_writable_size(size_t write_size) noexcept;
    virtual size_t write(const void *buf, size_t len) noexcept override;

    /**
     * 在数据前面添加一个 header 来存储数据大小，并设置读指针到 header
     */
    void raw_pack() noexcept;

    /**
     * header big endian to host endian
     */
    static header_type header_betoh(header_type header) noexcept;

    /**
     * 设置读、写指针到 header，准备写入 header
     */
    void raw_rewind() noexcept;

private:
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;

private:
    uint8_t *_buffer = nullptr;
    size_t _capacity = 0;

    size_t _read_index = sizeof(header_type);
    size_t _write_index = sizeof(header_type);

    bool _little_endian = false; // Big endian for network
};

}

#endif
