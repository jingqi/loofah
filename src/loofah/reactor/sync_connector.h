
#ifndef ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_
#define ___HEADFILE_A2DA8CDE_AA0C_40E3_A329_AC8F4DB3B8A2_

#include "sync_stream.h"
#include "../inet_addr.h"

namespace loofah
{

class LOOFAH_API SyncConnector
{
public:
    bool connect(SyncStream *stream, const INETAddr& address);
};

}

#endif
