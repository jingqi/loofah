
#ifndef ___HEADFILE_69C06121_0AC2_4D52_A730_FD4112450B43_
#define ___HEADFILE_69C06121_0AC2_4D52_A730_FD4112450B43_

#include "../inet_base/connector_base.h"

namespace loofah
{

template <typename CHANNEL>
class ProactConnector : public ConnectorBase
{
private:
    // Invalid methods
    ProactConnector();

public:
    static nut::rc_ptr<CHANNEL> connect(const InetAddr& address)
    {
        nut::rc_ptr<CHANNEL> channel = nut::rc_new<CHANNEL>();
        assert(nullptr != channel);
        channel->initialize();
        if (!ConnectorBase::connect(channel, address))
            channel.set_null();
        return channel;
    }
};

}

#endif
