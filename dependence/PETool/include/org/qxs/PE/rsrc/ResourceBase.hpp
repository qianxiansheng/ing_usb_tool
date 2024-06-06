#ifndef __ORG_QXS_PE_RSRC_RESOURCEBASE_H__
#define __ORG_QXS_PE_RSRC_RESOURCEBASE_H__

#include <vector>
#include <Windows.h>

namespace org::qxs::pe::rsrc {
    class ResourceBase {
    public:
        ResourceBase(int type) 
            : _type(type)
            , _id(0){
        }
    public:
        int _id;
        int _type;
        std::vector<uint8_t> _d;
    };
}

#endif