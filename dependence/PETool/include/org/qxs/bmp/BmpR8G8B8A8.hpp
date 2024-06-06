#ifndef __ORG_QXS_BMP_BMPR8G8B8A8_H__
#define __ORG_QXS_BMP_BMPR8G8B8A8_H__

#include <vector>

namespace org::qxs::bmp {
    class BmpR8G8B8A8 {
    public:
    public:
        int _w;
        int _h;
        int _num_channels;
        std::vector<uint8_t> _d;
    };
}

#endif