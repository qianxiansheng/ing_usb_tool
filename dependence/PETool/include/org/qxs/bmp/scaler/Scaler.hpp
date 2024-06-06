#ifndef __ORG_QXS_BMP_SCALER_SCALER_H__
#define __ORG_QXS_BMP_SCALER_SCALER_H__

#include <filesystem>
#include <memory>

#include "org/qxs/bmp/BmpR8G8B8A8.hpp"

namespace org::qxs::bmp::scaler {
    class Scaler {
    public:
        virtual BmpR8G8B8A8 scaler(const BmpR8G8B8A8& bmp, uint32_t dst_w, uint32_t dst_h) = 0;
        virtual ~Scaler() {}
    };
}

#endif