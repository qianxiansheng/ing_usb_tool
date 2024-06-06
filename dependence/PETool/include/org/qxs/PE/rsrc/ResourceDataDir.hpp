#ifndef __ORG_QXS_PE_RSRC_RESOURCEDATADIR_H__
#define __ORG_QXS_PE_RSRC_RESOURCEDATADIR_H__

#include "org/qxs/PE/rsrc/ResourceBase.hpp"
#include "org/qxs/PE/PESection.hpp"
#include "org/qxs/common/Common.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace org::qxs::pe::rsrc {
    class ResourceDataDir {
    public:
        PESection convertToData(uint32_t rva, uint32_t fp, uint32_t sectionAlign, uint32_t fileAlign)
        {
            PESection section;
            memset(&section, 0, sizeof(PESection));

            IMAGE_RESOURCE_DIRECTORY dir = { 0 };
            IMAGE_RESOURCE_DIRECTORY_ENTRY dirEntry = { 0 };
            IMAGE_RESOURCE_DATA_ENTRY dataEntry = { 0 };

            std::vector<ResourceBase> iconResource;
            std::copy_if(_rsrc_data_dir.begin(), _rsrc_data_dir.end(), std::back_inserter(iconResource), 
                [](ResourceBase& resourse) {
                    return resourse._type == 3;
                }
            );
            auto iconGroupResource = std::find_if(_rsrc_data_dir.begin(), _rsrc_data_dir.end(), 
                [](ResourceBase& resourse) {
                    return resourse._type == 14;
                }
            );
            auto manifestResource = std::find_if(_rsrc_data_dir.begin(), _rsrc_data_dir.end(),
                [](ResourceBase& resourse) {
                    return resourse._type == 24;
                }
            );

            dirEntry.DataIsDirectory = 1;
            dirEntry.NameIsString = 0;
            dirEntry.OffsetToDirectory = static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + 3 * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));

            std::ostringstream ss;

            /* first level */
            {
                /* dir */
                dir.NumberOfIdEntries = 3;
                ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                
                /* dir entry: Type */
                dirEntry.Id = 3;
                ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + iconResource.size() * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                
                dirEntry.Id = 14;
                ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                
                dirEntry.Id = 24;
                ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
            }

            /* second level */
            {
                /* dir: Icon */
                dir.NumberOfIdEntries = static_cast<uint16_t>(iconResource.size());
                ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                /* dir entry */
                for (size_t i = 0; i < iconResource.size(); ++i) {
                    dirEntry.Id = iconResource[i]._id;
                    ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                    dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                }

                /* dir: Icon Group */
                dir.NumberOfIdEntries = 1;
                ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                /* dir entry */
                dirEntry.Id = iconGroupResource->_id;
                ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));

                /* dir: Icon Manifest */
                dir.NumberOfIdEntries = 1;
                ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                /* dir entry */
                dirEntry.Id = manifestResource->_id;
                ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                dirEntry.OffsetToDirectory += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
            }

            dir.NumberOfIdEntries = 1;
            dirEntry.OffsetToData = dirEntry.OffsetToDirectory & 0x7FFFFFFF;

            /* third level */
            {
                /* Icon */
                for (size_t i = 0; i < iconResource.size(); ++i) {
                    /* dir */
                    ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                    /* dir entry */
                    ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                    dirEntry.OffsetToData += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                }
                /* Icon Group */
                {
                    /* dir */
                    ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                    /* dir entry */
                    ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                    dirEntry.OffsetToData += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                }
                /* Manifest */
                {
                    /* dir */
                    ss.write((char*)&dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
                    /* dir entry */
                    ss.write((char*)&dirEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
                    dirEntry.OffsetToData += static_cast<uint32_t>(sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                }
            }

            dirEntry.OffsetToDirectory = (dirEntry.OffsetToDirectory + 7) & ~7; // align to 8
            dataEntry.OffsetToData = dirEntry.OffsetToDirectory + rva;

            /* sct data */
            {
                /* Icon */
                for (size_t i = 0; i < iconResource.size(); ++i) {
                    dataEntry.Size = static_cast<uint32_t>(iconResource[i]._d.size());
                    ss.write((char*)&dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                    dataEntry.OffsetToData += dataEntry.Size;
                    dataEntry.OffsetToData = (dataEntry.OffsetToData + 7) & ~7; // align to 8
                }
                /* Icon Group */
                {
                    dataEntry.Size = static_cast<uint32_t>(iconGroupResource->_d.size());
                    ss.write((char*)&dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                    dataEntry.OffsetToData += dataEntry.Size;
                    dataEntry.OffsetToData = (dataEntry.OffsetToData + 7) & ~7; // align to 8
                }
                /* Manifest */
                {
                    dataEntry.Size = static_cast<uint32_t>(manifestResource->_d.size());
                    ss.write((char*)&dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY));
                    dataEntry.OffsetToData += dataEntry.Size;
                    dataEntry.OffsetToData = (dataEntry.OffsetToData + 7) & ~7; // align to 8
                }
            }

            std::streampos length = ss.tellp();
            std::vector<uint8_t> dummy(dirEntry.OffsetToDirectory - static_cast<uint32_t>(length));
            memset(dummy.data(), 0, dummy.size());
            ss.write((char*)dummy.data(), dummy.size());

            /* data */
            {
                /* Icon */
                for (size_t i = 0; i < iconResource.size(); ++i) {
                    ss.write((char*)iconResource[i]._d.data(), iconResource[i]._d.size());
                    dirEntry.OffsetToDirectory += static_cast<uint32_t>(iconResource[i]._d.size());

                    if (dirEntry.OffsetToDirectory & 7) {
                        dirEntry.OffsetToDirectory = (dirEntry.OffsetToDirectory + 7) & ~7; // align to 8

                        std::streampos length = ss.tellp();
                        dummy.resize(dirEntry.OffsetToDirectory - static_cast<uint32_t>(length));
                        memset(dummy.data(), 0, dummy.size());
                        ss.write((char*)dummy.data(), dummy.size());
                    }
                }
                /* Icon Group */
                {
                    ss.write((char*)iconGroupResource->_d.data(), iconGroupResource->_d.size());
                    dirEntry.OffsetToDirectory += static_cast<uint32_t>(iconGroupResource->_d.size());

                    if (dirEntry.OffsetToDirectory & 7) {
                        dirEntry.OffsetToDirectory = (dirEntry.OffsetToDirectory + 7) & ~7; // align to 8

                        std::streampos length = ss.tellp();
                        dummy.resize(dirEntry.OffsetToDirectory - static_cast<uint32_t>(length));
                        memset(dummy.data(), 0, dummy.size());
                        ss.write((char*)dummy.data(), dummy.size());
                    }
                }
                /* Manifest */
                {
                    ss.write((char*)manifestResource->_d.data(), manifestResource->_d.size());
                    dirEntry.OffsetToDirectory += static_cast<uint32_t>(manifestResource->_d.size());

                    if (dirEntry.OffsetToDirectory & 7) {
                        dirEntry.OffsetToDirectory = (dirEntry.OffsetToDirectory + 7) & ~7; // align to 8

                        std::streampos length = ss.tellp();
                        dummy.resize(dirEntry.OffsetToDirectory - static_cast<uint32_t>(length));
                        memset(dummy.data(), 0, dummy.size());
                        ss.write((char*)dummy.data(), dummy.size());
                    }
                }
            }
            ss.flush();

            strcpy_s((char*)section._section_header.Name, IMAGE_SIZEOF_SHORT_NAME, ".rsrc");
            section._section_header.Misc.VirtualSize = dataEntry.OffsetToData - rva;
            section._section_header.VirtualAddress = rva;
            section._section_header.SizeOfRawData = org::qxs::common::round_up(dataEntry.OffsetToData - rva, fileAlign);
            section._section_header.PointerToRawData = fp;
            section._section_header.PointerToRelocations = 0x00000000;
            section._section_header.PointerToLinenumbers = 0x00000000;
            section._section_header.NumberOfRelocations = 0x0000;
            section._section_header.NumberOfLinenumbers = 0x0000;
            section._section_header.Characteristics = 0x40000040;

            std::string str = ss.str();
            section._d = std::vector<uint8_t>(str.begin(), str.end());

            return section;
        }

    public:
        std::vector<ResourceBase> _rsrc_data_dir;
    };
}

#endif