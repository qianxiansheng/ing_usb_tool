#ifndef __ORG_QXS_PE_RSRC_RESOURCEICONGROUP_H__
#define __ORG_QXS_PE_RSRC_RESOURCEICONGROUP_H__

#include <vector>
#include "org/qxs/PE/rsrc/ResourceBase.hpp"

namespace org::qxs::pe::rsrc {
    class ResourceIconGroup : public ResourceBase {
    public:
        ResourceIconGroup() : ResourceBase(14) {}
    public:
        virtual ~ResourceIconGroup() {}

    public:
        std::vector<int> _icon_ids;
    };
}

#endif