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

	//��ͼ���ļ�
	hIconFile = CreateFile(lpIconFile, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIconFile == INVALID_HANDLE_VALUE)
	{
		return;
	}
	//��ȡ�ļ�ͷ����Ϣ
	ret = ReadFile(hIconFile, &header, sizeof(HEADER), &dwReserved, NULL);
	if (!ret)
	{
		CloseHandle(hIconFile);
		return;
	}
	//����ÿһ��ͼ���Ŀ¼��Ϣ�������
	pIconDirEntry = (LPICONDIRENTRY)new BYTE[header.idCount * sizeof(ICONDIRENTRY)];
	if (pIconDirEntry == NULL)
	{
		CloseHandle(hIconFile);
		return;
	}
	//��Icon�ļ��ж�ȡÿһ��ͼ���Ŀ¼��Ϣ
	ret = ReadFile(hIconFile, pIconDirEntry, header.idCount * sizeof(ICONDIRENTRY), &dwReserved, NULL);
	if (!ret)
	{
		delete[] pIconDirEntry;
		CloseHandle(hIconFile);
		return;
	}
	//����EXE�ļ���RT_GROUP_ICON��������ݽṹ�������
	nGSize = sizeof(GRPICONDIR) + header.idCount * sizeof(ICONDIRENTRY);
	pGrpIconDir = (LPGRPICONDIR)new BYTE[nGSize];
	//�����Ϣ�������൱��һ��ת���Ĺ���
	pGrpIconDir->idReserved = header.idReserved;
	pGrpIconDir->idType = header.idType;
	pGrpIconDir->idCount = header.idCount;
	//������Ϣ������ÿһ��ͼ���Ӧ��ID��IDΪλ��������
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
	//��ʼ����EXE�е�ͼ����Դ��ID��Ϊ��С�������ԭ�����ڣ�ID��ͼ����Ϣ���滻Ϊ�µġ�
	hUpdate = BeginUpdateResource(lpExeName, false);
	if (hUpdate != NULL)
	{
		//���ȸ���RT_GROUP_ICON��Ϣ
		ret = UpdateResource(hUpdate, RT_GROUP_ICON, MAKEINTRESOURCE(0), MAKELANGID(LANG_CHINESE, SUBLANG_SYS_DEFAULT), (LPVOID)pGrpIconDir, nGSize);
		if (!ret)
		{
			delete[] pIconDirEntry;
			delete[] pGrpIconDir;
			CloseHandle(hIconFile);
			return;
		}
		//���ŵ���ÿһ��Icon����Ϣ���
		for (i = 0; i < header.idCount; i++)
		{
			//Icon���ֽ���
			nSize = pIconDirEntry[i].dwBytesInRes;
			//ƫ���ļ���ָ�뵽��ǰͼ��Ŀ�ʼ��
			dwRet = SetFilePointer(hIconFile, pIconDirEntry[i].dwImageOffset, NULL, FILE_BEGIN);
			if (dwRet == INVALID_SET_FILE_POINTER)
			{
				break;
			}
			//׼��pIconBytes������ļ����Byte��Ϣ���ڸ��µ�EXE�С�
			delete[] pIconBytes;
			pIconBytes = new BYTE[nSize];
			ret = ReadFile(hIconFile, (LPVOID)pIconBytes, nSize, &dwReserved, NULL);
			if (!ret)
			{
				break;
			}
			//����ÿһ��ID��Ӧ��RT_ICON��Ϣ
			ret = UpdateResource(hUpdate, RT_ICON, MAKEINTRESOURCE(pGrpIconDir->idEntries[i].nID), MAKELANGID(LANG_CHINESE, SUBLANG_SYS_DEFAULT), (LPVOID)pIconBytes, nSize);
			if (!ret)
			{
				break;
			}
		}
		//����EXE��Դ�ĸ��²���
		if (pIconBytes != NULL)
		{
			delete[] pIconBytes;
		}
		EndUpdateResource(hUpdate, false);
	}
	//������Դ���ر�Icon�ļ������˸��²���������
	delete[] pGrpIconDir;
	delete[] pIconDirEntry;
	CloseHandle(hIconFile);
}