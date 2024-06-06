#ifndef __ORG_QXS_PE_ICONTOOL_H__
#define __ORG_QXS_PE_ICONTOOL_H__

#include <vector>

#include "org/qxs/bmp/BmpGroup.hpp"
#include "org/qxs/PE/PEFile.hpp"
#include "org/qxs/PE/rsrc/ResourceIcon.hpp"

using namespace org::qxs::bmp;
using namespace org::qxs::pe::rsrc;

namespace org::qxs::pe {
    class IconTool {
    public:
        static void replaceIcon(PEFile& file, std::vector<ResourceBase> icons) {
            auto& section = file._sections[4];
            auto buffer = section._d.data();
            PIMAGE_RESOURCE_DIRECTORY pFirst = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY>(buffer);
            PIMAGE_RESOURCE_DIRECTORY_ENTRY pFirstEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pFirst + 1);
            for (uint16_t i = 0; i < pFirst->NumberOfIdEntries; ++i) {

                if (pFirstEntry[i].Id != 3)
                    continue;
                
                PIMAGE_RESOURCE_DIRECTORY pSecond = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY>(buffer + pFirstEntry[i].OffsetToDirectory);
                PIMAGE_RESOURCE_DIRECTORY_ENTRY pSecondEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pSecond + 1);
            
                for (uint16_t j = 0; j < pSecond->NumberOfIdEntries; ++j) {

                    PIMAGE_RESOURCE_DIRECTORY pThird = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY>(buffer + pSecondEntry[j].OffsetToDirectory);
                    PIMAGE_RESOURCE_DIRECTORY_ENTRY pThirdEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pThird + 1);

                    PIMAGE_RESOURCE_DATA_ENTRY pStcData = reinterpret_cast<PIMAGE_RESOURCE_DATA_ENTRY>(buffer + pThirdEntry[0].OffsetToData);
                    uint64_t StcDataOffset = pStcData->OffsetToData - section._section_header.VirtualAddress;
                    
                    uint8_t* pResBuf = (buffer + StcDataOffset);
                    PBITMAPINFOHEADER bitmapHeader = reinterpret_cast<PBITMAPINFOHEADER>(pResBuf);
                    uint8_t* pBitmapData = pResBuf + sizeof(BITMAPINFOHEADER);

                    for (size_t k = 0; k < icons.size(); ++k) {
                        PBITMAPINFOHEADER iconBitmapHeader = reinterpret_cast<PBITMAPINFOHEADER>(icons[k]._d.data());
                        uint8_t* pIconBitmapData = icons[k]._d.data() + sizeof(BITMAPINFOHEADER);

                        if (bitmapHeader->biWidth == iconBitmapHeader->biWidth && 
                            bitmapHeader->biHeight == iconBitmapHeader->biHeight) {
                            size_t size = bitmapHeader->biWidth * bitmapHeader->biWidth * 4;
                            memcpy(pBitmapData, pIconBitmapData, size);
                        }
                    }
                }
            }
        }
    };
}

#endif