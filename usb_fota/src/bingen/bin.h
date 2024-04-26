#pragma once

#include "pch.h"
#include <iostream>
#include <filesystem>

enum check_type_e
{
	CHECK_TYPE_CRC = 0,
	CHECK_TYPE_SUM = 1,
};
enum upgrade_type_e
{
	UPGRADE_TYPE_APP_ONLY = 0,				// ������APPӦ�ó���
	UPGRADE_TYPE_PLATFORM_APP = 1,			// ����platform+APP����
};
enum encryption_type_e
{
	BIN_ENCRYPTION_TYPE_XOR = 0,
	BIN_ENCRYPTION_TYPE_AES = 1,
};

struct bin_info_t
{
	char name[BIN_NAME_BUFF_MAX_SIZE];
	char name_gbk[BIN_NAME_BUFF_MAX_SIZE];
	char load_address[9];
	uint32_t load_addr;	    // bin�ļ�Ŀ��λ��     0x0202A000/0x02003000
	uint32_t size;			// ���� data->size
	std::vector<uint8_t> data;
};

struct bin_config_t
{
	char identify[9];		// ͷ�ļ���ʶ��	 8 + 1	"INGCHIPS"
	char chip_code[16];		// оƬ����		15 + 1  "ING91683C_TB"
	char proj_code[24];		// ��Ŀ����		23 + 1	"HS_KB"
	char hard_version[7];	// Ӳ���汾		 6 + 1	"V2.1.3"
	char soft_version[7];	// ����汾		 6 + 1	"V2.1.3"
	int  check_type;		// У������
	int  check_len;			// ����������
	int  check_value;		// У����
	uint16_t  block_size;	// �������С
	uint16_t  block_num;	// ����������
	int	 upgrade_type;		// ��������

	bool encryption_enable;	// ʹ�ܼ���
	int	 encryption_type;	// ��������
	int	 encryption_keylen;	// ��Կ����
	char encryption_xor[33];// XOR vector
	unsigned char enc_xor[16];
	char encryption_key[33];// AES key
	unsigned char enc_key[16];
	char encryption_iv[33];	// AES iv
	unsigned char enc_iv[16];

	//======================//====================================

	int out_data_crc;
	uint32_t out_data_load_addr;
	std::vector<uint8_t> out_data;

	bin_info_t platform;
	bin_info_t app;
};


void Saveini(std::filesystem::path ini_path);

void Loadini(std::filesystem::path ini_path);

void Exportbin(std::filesystem::path export_bin_path);

void Alsbin(std::filesystem::path bin_path);