#pragma once

#include <Windows.h>

typedef struct tagHEADER
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
}HEADER, * LPHEADER;

typedef struct tagICONDIRENTRY
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
}ICONDIRENTRY, * LPICONDIRENTRY;

#pragma pack( push )
#pragma pack( 2 )
typedef struct tagGRPICONDIRENTRY
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD nID;
}GRPICONDIRENTRY, * LPGRPICONDIRENTRY;;

typedef struct tagGRPICONDIR
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
    GRPICONDIRENTRY idEntries[1];
}GRPICONDIR, * LPGRPICONDIR;
#pragma pack( pop )

void changedExeIcon(LPCTSTR lpExeName, LPCTSTR lpIconFile);