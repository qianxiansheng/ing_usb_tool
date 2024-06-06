#ifndef __ORG_QXS_BMP_ICOCONVERTER_H__
#define __ORG_QXS_BMP_ICOCONVERTER_H__

#include <iostream>

#include "BmpGroup.hpp"
#include "Converter.hpp"

namespace org::qxs::bmp {
    class PEResourceConverter : public Converter {
    public:
        void convert(const BmpGroup& bitmap_group, std::ostream& stream) override;
    };
}

#endif