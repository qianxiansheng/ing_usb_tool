#ifndef __ORG_QXS_BMP_BMPLOADER_H__
#define __ORG_QXS_BMP_BMPLOADER_H__

#include <memory>
#include <filesystem>
#include "Loader.hpp"
#include "BmpGroup.hpp"

namespace org::qxs::bmp {
    class BMPLoader : public Loader {
    public:
        ::std::shared_ptr<BmpGroup> load_image(const ::std::filesystem::path path) override;
    };
}

#endif