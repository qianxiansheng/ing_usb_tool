#ifndef __ORG_QXS_PE_PESECTION_H__
#define __ORG_QXS_PE_PESECTION_H__

#include <Windows.h>
#include <vector>

namespace org::qxs::pe {
    class PESection {
    public:
        IMAGE_SECTION_HEADER _section_header;
        std::vector<uint8_t> _d;
    };
}

#endif