#ifndef __ORG_QXS_BMP_SCALER_BILINEARINTERPOLATORSCALER_H__
#define __ORG_QXS_BMP_SCALER_BILINEARINTERPOLATORSCALER_H__

#include "org/qxs/bmp/scaler/Scaler.hpp"

namespace org::qxs::bmp::scaler {
    class BilinearInterpolatorScaler : public Scaler {
    public:
        float interpolation(int a, int b, float u);

        BmpR8G8B8A8 scaler(const BmpR8G8B8A8& bmp, uint32_t targetWidth, uint32_t targetHeight) override;
    };
}

#endif