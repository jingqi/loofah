
#ifndef ___HEADFILE_FD09D825_E70C_4211_A84A_4F9272410C36_
#define ___HEADFILE_FD09D825_E70C_4211_A84A_4F9272410C36_

#include "../loofah_config.h"

namespace loofah
{

class Channel
{
public:
    virtual void open(socket_t sock_fd) = 0;
};

}

#endif
