
#ifndef ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_
#define ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_

#include "../inet_base/connector_base.h"

namespace loofah
{

template <typename CHANNEL>
class ReactConnector : public ConnectorBase
{
private:
    // Invalid methods
    ReactConnector() = delete;

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
