
#ifndef ___HEADFILE_ACDB5769_D339_4965_A31F_43675C2E9173_
#define ___HEADFILE_ACDB5769_D339_4965_A31F_43675C2E9173_

namespace loofah
{

class ConnectionHandler
{
public:
    virtual ~ConnectionHandler() {}

    virtual int handle_connected() = 0;
    virtual int handle_disconnect() = 0;
    virtual int handle_read() = 0;
    virtual int handle_write() = 0;
    virtual int handle_error() = 0;
};

}

#endif
