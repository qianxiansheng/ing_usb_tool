#ifndef __ORG_QXS_PE_PEFILE_H__
#define __ORG_QXS_PE_PEFILE_H__

#include <vector>

#include "org/qxs/PE/PESection.hpp"

namespace org::qxs::pe {
    class PEFile {
    public:
        IMAGE_DOS_HEADER _dos;
        IMAGE_NT_HEADERS _nt;
        std::vector<uint8_t> _misc;
        std::vector<PESection> _sections;
    };
}

#endif