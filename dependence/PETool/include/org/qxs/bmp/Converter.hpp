#ifndef __ORG_QXS_BMP_CONVERTER_H__
#define __ORG_QXS_BMP_CONVERTER_H__

#include <iostream>
#include <memory>

#include "BmpGroup.hpp"

namespace org::qxs::bmp {
    class Converter {
    public:
        virtual void convert(const BmpGroup& bitmap_group, ::std::ostream& stream) = 0;
        virtual ~Converter() {}
    };
}

#endif