
#ifndef ___HEADFILE_8DAEC806_FFB8_4415_A331_8D4965371079_
#define ___HEADFILE_8DAEC806_FFB8_4415_A331_8D4965371079_

#include <stdint.h>
#include <string.h> // for size_t

namespace loofah
{

/**
 * Ring buffer
 */
class RingBuffer
{
    /* 环形存储:
     *
      一般状态下
              |=>>>>>>>>>>>>>|
      |-------|++++++++++++++|-------------|
      0   read_index   write_index      capacity

      环写状态下
      |>>>>>>>>>|                  |=>>>>>>>>>>|
      |+++++++++|------------------|+++++++++++|
      0    write_index         read_index    capacity

     */

    void *_buffer = NULL;
    size_t _capacity = 0;
    size_t _read_index = 0, _write_index = 0;

private:
    void ensure_writable_size(size_t write_size);

public:
    RingBuffer();
    RingBuffer(const RingBuffer& x);
    ~RingBuffer();
    RingBuffer& operator=(const RingBuffer& x);

    void clear();

    size_t readable_size() const;
    size_t read(void *buf, size_t size);
    size_t look_ahead(void *buf, size_t size) const;
    size_t skip_read(size_t size);

    /**
     * 返回可读指针
     *
     * @return 0 所有指针都无效
     *         1 第一个指针有效
     *         2 第一个、第二个指针有效
     */
    int readable_pointers(const void **buf1, size_t *size1,
                          const void **buf2, size_t *size2) const;

    size_t writable_size() const;
    void write(const void *buf, size_t size);

    /**
     * 返回可写指针
     *
     * @return 0 所有指针都无效
     *         1 第一个指针有效
     *         2 第一个、第二个指针有效
     */
    int writable_pointers(void **buf1, size_t *size1,
                          void **buf2, size_t *size2);
};

}

#endif
