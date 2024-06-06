#ifndef __ORG_QXS_PE_PELOADER_H__
#define __ORG_QXS_PE_PELOADER_H__

#include <vector>
#include <filesystem>

#include "org/qxs/PE/PEFile.hpp"

namespace org::qxs::pe {
    class PELoader {
    public:
        static PEFile loadFile(std::filesystem::path path);
        static PEFile loadFile(std::vector<uint8_t>& d);
    };
}

#endif