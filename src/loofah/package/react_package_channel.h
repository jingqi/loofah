
#ifndef ___HEADFILE_54F36F61_E321_40AB_A45B_61513B51FB12_
#define ___HEADFILE_54F36F61_E321_40AB_A45B_61513B51FB12_

#include "../loofah_config.h"

#include <list>

#include <nut/time/time_wheel.h>
#include <nut/debugging/destroy_checker.h>

#include "../reactor/react_channel.h"
#include "../reactor/reactor.h"
#include "package_channel_base.h"


namespace loofah
{

class LOOFAH_API ReactPackageChannel : public ReactChannel, public PackageChannelBase
{
    NUT_REF_COUNTABLE_OVERRIDE

public:
    void set_reactor(Reactor *reactor);

    virtual SockStream& get_sock_stream() final override;

    /**
     * 写数据
     */
    virtual void write(Package *pkg) final override;

    /**
     * 关闭连接
     *
     * @param discard_write 是否忽略尚未写入的 package, 否则等待全部写入后再关闭
     */
    virtual void close(int err = 0, bool discard_write = false) final override;

public:
    /**
     * ReactChannel 接口实现
     */
    virtual void open(socket_t fd) override;
    virtual void handle_channel_connected() final override;

    virtual void handle_read_ready() final override;
    virtual void handle_write_ready() final override;
    virtual void handle_io_error(int err) final override;

private:
    // 关闭连接
    virtual void force_close(int err) final override;
};

}

#endif
