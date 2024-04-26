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
	UPGRADE_TYPE_APP_ONLY = 0,				// 仅升级APP应用程序
	UPGRADE_TYPE_PLATFORM_APP = 1,			// 升级platform+APP程序
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
	uint32_t load_addr;	    // bin文件目标位置     0x0202A000/0x02003000
	uint32_t size;			// 冗余 data->size
	std::vector<uint8_t> data;
};

struct bin_config_t
{
	char identify[9];		// 头文件标识符	 8 + 1	"INGCHIPS"
	char chip_code[16];		// 芯片代号		15 + 1  "ING91683C_TB"
	char proj_code[24];		// 项目代号		23 + 1	"HS_KB"
	char hard_version[7];	// 硬件版本		 6 + 1	"V2.1.3"
	char soft_version[7];	// 软件版本		 6 + 1	"V2.1.3"
	int  check_type;		// 校验类型
	int  check_len;			// 检验结果长度
	int  check_value;		// 校验结果
	uint16_t  block_size;	// 升级块大小
	uint16_t  block_num;	// 升级块总数
	int	 upgrade_type;		// 升级类型

	bool encryption_enable;	// 使能加密
	int	 encryption_type;	// 加密类型
	int	 encryption_keylen;	// 密钥长度
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