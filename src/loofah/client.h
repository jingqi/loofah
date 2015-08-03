
#ifndef ___HEADFILE_37473EB6_C0EA_4343_A88A_5F86FEE16A9B_
#define ___HEADFILE_37473EB6_C0EA_4343_A88A_5F86FEE16A9B_

#include <string>

namespace loofah
{

class Client
{
public:
    void init(const std::string remote_addr, short port);
    void connect();
};

}

#endif
