
#ifndef ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_
#define ___HEADFILE_C2BE4FDE_CF83_4810_ABF9_001B61183CC3_

#include <list>

#include "../proactor/proact_channel.h"
#include "../proactor/proactor.h"
#include "package.h"
#include "fregment_buffer.h"

namespace loofah
{

class PackageChannel : public ProactChannel
{
    Proactor *_proactor = NULL;

    typedef std::list<nut::rc_ptr<Package> > queue_t;
    queue_t _write_queue;

    FregmentBuffer::Fregment *_read_freg = NULL;
    FregmentBuffer _readed_buffer;

private:
    void launch_read();
    void launch_write();

public:
    PackageChannel(Proactor *proactor);
    ~PackageChannel();

    virtual void open(socket_t fd) final override;
    virtual void handle_read_completed(int cb) final override;
    virtual void handle_write_completed(int cb) final override;

    virtual void handle_close() = 0;
    virtual void handle_read(nut::rc_ptr<Package> pkg) = 0;

    void write(nut::rc_ptr<Package> pkg);
};

}

#endif
