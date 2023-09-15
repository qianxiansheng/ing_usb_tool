#include "icon.h"

void changedExeIcon(LPCTSTR lpExeName, LPCTSTR lpIconFile)
{
	LPICONDIRENTRY pIconDirEntry(NULL);
	LPGRPICONDIR pGrpIconDir(NULL);
	HEADER header;
	LPBYTE pIconBytes(NULL);
	HANDLE hIconFile(NULL);
	DWORD dwRet(0), nSize(0), nGSize(0), dwReserved(0);
	HANDLE hUpdate(NULL);
	BOOL ret(FALSE);
	WORD i(0);

	//打开图标文件
	hIconFile = CreateFile(lpIconFile, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIconFile == INVALID_HANDLE_VALUE)
	{
		return;
	}
	//读取文件头部信息
	ret = ReadFile(hIconFile, &header, sizeof(HEADER), &dwReserved, NULL);
	if (!ret)
	{
		CloseHandle(hIconFile);
		return;
	}
	//建立每一个图标的目录信息存放区域
	pIconDirEntry = (LPICONDIRENTRY)new BYTE[header.idCount * sizeof(ICONDIRENTRY)];
	if (pIconDirEntry == NULL)
	{
		CloseHandle(hIconFile);
		return;
	}
	//从Icon文件中读取每一个图标的目录信息
	ret = ReadFile(hIconFile, pIconDirEntry, header.idCount * sizeof(ICONDIRENTRY), &dwReserved, NULL);
	if (!ret)
	{
		delete[] pIconDirEntry;
		CloseHandle(hIconFile);
		return;
	}
	//建立EXE文件中RT_GROUP_ICON所需的数据结构存放区域
	nGSize = sizeof(GRPICONDIR) + header.idCount * sizeof(ICONDIRENTRY);
	pGrpIconDir = (LPGRPICONDIR)new BYTE[nGSize];
	//填充信息，这里相当于一个转换的过程
	pGrpIconDir->idReserved = header.idReserved;
	pGrpIconDir->idType = header.idType;
	pGrpIconDir->idCount = header.idCount;
	//复制信息并设置每一个图标对应的ID。ID为位置索引号
	for (i = 0; i < header.idCount; i++)
	{
		pGrpIconDir->idEntries[i].bWidth = pIconDirEntry[i].bWidth;
		pGrpIconDir->idEntries[i].bHeight = pIconDirEntry[i].bHeight;
		pGrpIconDir->idEntries[i].bColorCount = pIconDirEntry[i].bColorCount;
		pGrpIconDir->idEntries[i].bReserved = pIconDirEntry[i].bReserved;
		pGrpIconDir->idEntries[i].wPlanes = pIconDirEntry[i].wPlanes;
		pGrpIconDir->idEntries[i].wBitCount = pIconDirEntry[i].wBitCount;
		pGrpIconDir->idEntries[i].dwBytesInRes = pIconDirEntry[i].dwBytesInRes;
		pGrpIconDir->idEntries[i].nID = i;
	}
	//开始更新EXE中的图标资源，ID定为最小０，如果原来存在０ID的图标信息则被替换为新的。
	hUpdate = BeginUpdateResource(lpExeName, false);
	if (hUpdate != NULL)
	{
		//首先更新RT_GROUP_ICON信息
		ret = UpdateResource(hUpdate, RT_GROUP_ICON, MAKEINTRESOURCE(0), MAKELANGID(LANG_CHINESE, SUBLANG_SYS_DEFAULT), (LPVOID)pGrpIconDir, nGSize);
		if (!ret)
		{
			delete[] pIconDirEntry;
			delete[] pGrpIconDir;
			CloseHandle(hIconFile);
			return;
		}
		//接着的是每一个Icon的信息存放
		for (i = 0; i < header.idCount; i++)
		{
			//Icon的字节数
			nSize = pIconDirEntry[i].dwBytesInRes;
			//偏移文件的指针到当前图标的开始处
			dwRet = SetFilePointer(hIconFile, pIconDirEntry[i].dwImageOffset, NULL, FILE_BEGIN);
			if (dwRet == INVALID_SET_FILE_POINTER)
			{
				break;
			}
			//准备pIconBytes来存放文件里的Byte信息用于更新到EXE中。
			delete[] pIconBytes;
			pIconBytes = new BYTE[nSize];
			ret = ReadFile(hIconFile, (LPVOID)pIconBytes, nSize, &dwReserved, NULL);
			if (!ret)
			{
				break;
			}
			//更新每一个ID对应的RT_ICON信息
			ret = UpdateResource(hUpdate, RT_ICON, MAKEINTRESOURCE(pGrpIconDir->idEntries[i].nID), MAKELANGID(LANG_CHINESE, SUBLANG_SYS_DEFAULT), (LPVOID)pIconBytes, nSize);
			if (!ret)
			{
				break;
			}
		}
		//结束EXE资源的更新操作
		if (pIconBytes != NULL)
		{
			delete[] pIconBytes;
		}
		EndUpdateResource(hUpdate, false);
	}
	//清理资源并关闭Icon文件，到此更新操作结束！
	delete[] pGrpIconDir;
	delete[] pIconDirEntry;
	CloseHandle(hIconFile);
}