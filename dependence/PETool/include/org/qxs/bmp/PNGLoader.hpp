#ifndef __ORG_QXS_BMP_PNGLOADER_H__
#define __ORG_QXS_BMP_PNGLOADER_H__

#include <memory>
#include <filesystem>
#include "Loader.hpp"
#include "BmpGroup.hpp"

namespace org::qxs::bmp {
    class PNGLoader : public Loader {
    public:
        ::std::shared_ptr<BmpGroup> load_image(const ::std::filesystem::path path) override;
    };
}

#endif