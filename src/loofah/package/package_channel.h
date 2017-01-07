
#ifndef ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_
#define ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_

#include "../loofah_config.h"

#include <list>

#include <nut/container/rwbuffer/fragment_buffer.h>

#include "../proactor/proact_channel.h"
#include "../proactor/proactor.h"
#include "package.h"

namespace loofah
{

class LOOFAH_API PackageChannel : public ProactChannel
{
    Proactor *_proactor = NULL;

    typedef std::list<nut::rc_ptr<Package> > queue_t;
    queue_t _write_queue;

    nut::FragmentBuffer::Fragment *_read_frag = NULL;
    nut::FragmentBuffer _readed_buffer;

private:
    void launch_read();
    void launch_write();
    void write(Package *pkg);

public:
    PackageChannel();
    virtual ~PackageChannel();

    void initialize(Proactor *proactor);

    virtual void open(socket_t fd) final override;
    virtual void handle_read_completed(int cb) final override;
    virtual void handle_write_completed(int cb) final override;

    virtual void handle_read(Package *pkg) = 0;
    virtual void handle_close() = 0;
    void async_write(Package *pkg);
};

}

#endif
