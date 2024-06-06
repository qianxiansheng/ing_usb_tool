#ifndef __ORG_QXS_PE_PECONVERTER_H__
#define __ORG_QXS_PE_PECONVERTER_H__

#include <vector>
#include <filesystem>

#include "org/qxs/PE/PEFile.hpp"

namespace org::qxs::pe {
    class PEConverter {
    public:
        static void convertFile(const PEFile& pr, std::filesystem::path path);
        static std::vector<uint8_t> convertFile(const PEFile& pr);
    };
}

#endif