
#ifndef ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_
#define ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_

#include "../loofah_config.h"

#include <list>

#include <nut/time/time_wheel.h>
#include <nut/container/rwbuffer/fragment_buffer.h>
#include <nut/debugging/destroy_checker.h>

#include "../proactor/proact_channel.h"
#include "../proactor/proactor.h"
#include "package_channel_base.h"


namespace loofah
{

class LOOFAH_API ProactPackageChannel : public ProactChannel, public PackageChannelBase
{
    NUT_REF_COUNTABLE_OVERRIDE

public:
    void set_proactor(Proactor *proactor);
    Proactor* get_proactor() const;

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
    virtual void close(bool discard_write = false) final override;

public:
    /**
     * ProactChannel 接口实现
     */
    virtual void open(socket_t fd) override;
    virtual void handle_channel_connected() final override;

    virtual void handle_read_completed(size_t cb) final override;
    virtual void handle_write_completed(size_t cb) final override;
    virtual void handle_io_exception(int err) override;

private:
    // 向 proactor 请求 read 操作
    void launch_read();
    // 向 proactor 请求 write 操作
    void launch_write();

    // 关闭连接
    virtual void force_close() final override;
};

}

#endif
