#ifndef __ORG_QXS_BMP_COLORSPACECONVERTER_H__
#define __ORG_QXS_BMP_COLORSPACECONVERTER_H__


#include <vector>
#include "BmpGroup.hpp"

namespace org::qxs::bmp {
    class ColorSpaceConverter {
    public:
        static std::vector<uint8_t> convert_to_bgra(const BmpR8G8B8A8& bitmap);
        static std::vector<uint8_t> convert_to_bgra_flipY(const BmpR8G8B8A8& bitmap);
        static std::vector<uint8_t> convert_to_bgra_flipY_bmp(const BmpR8G8B8A8& bitmap);
    };
}

#endif