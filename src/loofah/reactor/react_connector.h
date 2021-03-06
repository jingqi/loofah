﻿
#ifndef ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_
#define ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_

#include <assert.h>

#include <nut/rc/rc_new.h>

#include "../inet_base/inet_addr.h"
#include "react_handler.h"


namespace loofah
{

class ReactChannel;

class LOOFAH_API ReactConnectorBase
{
public:
    virtual ~ReactConnectorBase() = default;

    bool connect(Reactor *reactor, const InetAddr& address) noexcept;

protected:
    virtual nut::rc_ptr<ReactChannel> create_channel() noexcept = 0;
};

template <typename CHANNEL>
class ReactConnector : public ReactConnectorBase
{
protected:
    virtual nut::rc_ptr<ReactChannel> create_channel() noexcept final override
    {
        return nut::rc_new<CHANNEL>();
    }
};

}

#endif
