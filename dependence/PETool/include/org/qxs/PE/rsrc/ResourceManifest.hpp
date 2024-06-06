#ifndef __ORG_QXS_PE_RSRC_RESOURCEMANIFEST_H__
#define __ORG_QXS_PE_RSRC_RESOURCEMANIFEST_H__

#include <iostream>
#include <vector>
#include "org/qxs/PE/rsrc/ResourceBase.hpp"

namespace org::qxs::pe::rsrc {
    class ResourceManifest : public ResourceBase {
    public:
        ResourceManifest() : ResourceBase(24) {}
    public:
        virtual ~ResourceManifest() {}
    };
}

#endif