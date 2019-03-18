
#ifndef ___HEADFILE_FD09D825_E70C_4211_A84A_4F9272410C36_
#define ___HEADFILE_FD09D825_E70C_4211_A84A_4F9272410C36_

#include "../loofah_config.h"

#include <nut/rc/rc_ptr.h>


namespace loofah
{

class Channel
{
    NUT_REF_COUNTABLE

public:
    Channel() = default;
    virtual ~Channel() = default;

    /**
     * 初始化
     */
    virtual void initialize() = 0;

    /**
     * 设置 socket fd
     */
    virtual void open(socket_t sock_fd) = 0;

    /**
     * 链接已经建立
     */
    virtual void handle_channel_connected() = 0;

private:
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
};

}

#endif
