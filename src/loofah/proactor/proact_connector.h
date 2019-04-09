
#ifndef ___HEADFILE_69C06121_0AC2_4D52_A730_FD4112450B43_
#define ___HEADFILE_69C06121_0AC2_4D52_A730_FD4112450B43_

#include <assert.h>

#include <nut/rc/rc_new.h>

#include "../inet_base/inet_addr.h"
#include "proact_handler.h"


namespace loofah
{

class ProactChannel;

class LOOFAH_API ProactConnectorBase
{
public:
    virtual ~ProactConnectorBase() = default;

    bool connect(Proactor *proactor, const InetAddr& address);

protected:
    virtual nut::rc_ptr<ProactChannel> create_channel() = 0;
};

template <typename CHANNEL>
class ProactConnector : public ProactConnectorBase
{
protected:
    virtual nut::rc_ptr<ProactChannel> create_channel() final override
    {
        return nut::rc_new<CHANNEL>();
    }
};

}

#endif
