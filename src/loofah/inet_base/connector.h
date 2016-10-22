
#ifndef ___HEADFILE_1EDC4754_30D5_49B6_A4CD_6E501960F26A_
#define ___HEADFILE_1EDC4754_30D5_49B6_A4CD_6E501960F26A_

#include "channel.h"
#include "inet_addr.h"

namespace loofah
{

class LOOFAH_API Connector
{
public:
    bool connect(Channel *channel, const InetAddr& address);
};

}

#endif
