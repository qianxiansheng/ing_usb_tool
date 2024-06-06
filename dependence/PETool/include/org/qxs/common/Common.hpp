#ifndef __ORG_QXS_COMMON_COMMON_H__
#define __ORG_QXS_COMMON_COMMON_H__

#include <vector>
#include <filesystem>

namespace org::qxs::common {

    std::vector<uint8_t> loadFile(std::filesystem::path path);

    int round_up(int a, int b);
}

#endif