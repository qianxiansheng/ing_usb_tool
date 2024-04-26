#include "PEUtils.h"

#include <Windows.h>
#include <assert.h>

void LoadIconByPE(uint8_t* buffer, std::vector<uint8_t>& result, int size)
{
    // Dos Header
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)buffer;
    // PE Header
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(buffer + pDos->e_lfanew);
    // Section Table
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
    // PE Option Header
    PIMAGE_OPTIONAL_HEADER pOptionHeader = (PIMAGE_OPTIONAL_HEADER)&pNt->OptionalHeader;
    // Resource Directory Position
    PIMAGE_DATA_DIRECTORY pResDir = pOptionHeader->DataDirectory + IMAGE_DIRECTORY_ENTRY_RESOURCE;
    // IMAGE_DIRECTORY_SECTION
    uint64_t imageDirectoryFilePointer = 0;
    PIMAGE_SECTION_HEADER imageDirectorySection = NULL;
    for (int i = 0; i < pNt->FileHeader.NumberOfSections; ++i)
    {
        if (pResDir->VirtualAddress >= pSection[i].VirtualAddress &&
            pResDir->VirtualAddress < pSection[i].VirtualAddress + pSection[i].Misc.VirtualSize) {
            imageDirectoryFilePointer = pResDir->VirtualAddress - pSection[i].VirtualAddress + pSection[i].PointerToRawData;
            imageDirectorySection = &pSection[i];
            break;
        }
    }
    assert(imageDirectorySection);
    // Resource Directory Pointer
    uint8_t* pImageDirectory = buffer + imageDirectoryFilePointer;
    PIMAGE_RESOURCE_DIRECTORY pFirst = (PIMAGE_RESOURCE_DIRECTORY)(pImageDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pFirstEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pFirst + 1);
    for (uint16_t i = 0; i < pFirst->NumberOfIdEntries; ++i) {

        if (pFirstEntry[i].Id != 3)
            continue;

        PIMAGE_RESOURCE_DIRECTORY pSecond = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY>(pImageDirectory + pFirstEntry[i].OffsetToDirectory);
        PIMAGE_RESOURCE_DIRECTORY_ENTRY pSecondEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pSecond + 1);

        for (uint16_t j = 0; j < pSecond->NumberOfIdEntries; ++j) {

            PIMAGE_RESOURCE_DIRECTORY pThird = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY>(pImageDirectory + pSecondEntry[j].OffsetToDirectory);
            PIMAGE_RESOURCE_DIRECTORY_ENTRY pThirdEntry = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(pThird + 1);

            PIMAGE_RESOURCE_DATA_ENTRY pStcData = reinterpret_cast<PIMAGE_RESOURCE_DATA_ENTRY>(pImageDirectory + pThirdEntry[0].OffsetToData);
            uint32_t StcDataOffset = pStcData->OffsetToData - imageDirectorySection->VirtualAddress;

            uint8_t* pResBuf = (pImageDirectory + StcDataOffset);
            PBITMAPINFOHEADER bitmapHeader = (PBITMAPINFOHEADER)pResBuf;
            uint8_t* pBitmapData = pResBuf + sizeof(BITMAPINFOHEADER);

            if (bitmapHeader->biWidth == size) {
                size_t size = bitmapHeader->biWidth * bitmapHeader->biWidth * 4;
                result.resize(size);
                memcpy(result.data(), pBitmapData, size);
            }
        }
    }
}
