#ifndef __ORG_QXS_PE_RSRC_RESOURCEICON_H__
#define __ORG_QXS_PE_RSRC_RESOURCEICON_H__

#include <vector>
#include "org/qxs/PE/rsrc/ResourceBase.hpp"

namespace org::qxs::pe::rsrc {
    class ResourceIcon : public ResourceBase {
    public:
        ResourceIcon() : ResourceBase(3) {}
    public:
        virtual ~ResourceIcon() {}
    };
}

#endif