#ifndef __ORG_QXS_BMP_LOADER_H__
#define __ORG_QXS_BMP_LOADER_H__

#include <filesystem>
#include <memory>

#include "BmpGroup.hpp"

namespace org::qxs::bmp {
    class Loader {
    public:
        virtual ::std::shared_ptr<BmpGroup> load_image(const ::std::filesystem::path path) = 0;
        virtual ~Loader() {}
    };
}

#endif