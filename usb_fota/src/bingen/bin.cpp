#include "bin.h"

#include <iostream>
#include <fstream>
#include "util/INIReader.h"
#include "util/utils.h"

#include "imgui/extensions/ImConsole.h"

bin_config_t bin_config = {0};

extern ImLogger* logger;

void Saveini(std::filesystem::path ini_path)
{
	bin_config_t& c = bin_config;

	std::ofstream out;
	out.open(ini_path, std::ios::out);

	out << "[option]" << std::endl;
	out << "identify=" << c.identify << std::endl;
	out << "chip_code=" << c.chip_code << std::endl;
	out << "proj_code=" << c.proj_code << std::endl;
	out << "hard_version=" << c.hard_version << std::endl;
	out << "soft_version=" << c.soft_version << std::endl;
	out << "block_size=" << c.block_size << std::endl;
	out << "check_type=" << c.check_type << std::endl;
	out << "upgrade_type=" << (int)c.upgrade_type << std::endl;
	out << "encryption_enable=" << (int)c.encryption_enable << std::endl;
	out << "encryption_type=" << (int)c.encryption_type << std::endl;
	out << "encryption_xor=" << c.encryption_xor << std::endl;
	out << "encryption_key=" << c.encryption_key << std::endl;
	out << "encryption_iv=" << c.encryption_iv << std::endl;
	out << std::endl;

	out << "[app]" << std::endl;
	out << "name=" << c.in_name_gbk << std::endl;
	out << std::endl;

	out.close();
}

void Loadini(std::filesystem::path ini_path)
{
	bin_config_t& c = bin_config;

	INIReader reader(ini_path.u8string());
	
	strcpy(c.identify,		  reader.Get("option", "identify", BIN_CONFIG_DEFAULT_IDENTIFY).c_str());
	strcpy(c.chip_code,		  reader.Get("option", "chip_code", "--").c_str());
	strcpy(c.proj_code,		  reader.Get("option", "proj_code", "--").c_str());
	strcpy(c.hard_version,	  reader.Get("option", "hard_version", "V0.0.0").c_str());
	strcpy(c.soft_version,	  reader.Get("option", "soft_version", "V0.0.0").c_str());

	c.block_size			= (uint16_t)reader.GetInteger("option", "block_size", 64);
	c.check_type			= reader.GetInteger("option", "check_type", CHECK_TYPE_CRC);
	c.upgrade_type			= reader.GetInteger("option", "upgrade_type", UPGRADE_TYPE_APP_ONLY);
	c.encryption_enable		= reader.GetBoolean("option", "encryption_enable", true);
	c.encryption_type		= reader.GetInteger("option", "encryption_type", BIN_ENCRYPTION_TYPE_AES);
	strcpy(c.encryption_xor,  reader.Get("option", "encryption_xor", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF").c_str());
	strcpy(c.encryption_key,  reader.Get("option", "encryption_key", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF").c_str());
	strcpy(c.encryption_iv,   reader.Get("option", "encryption_iv",  "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF").c_str());

	strcpy(c.in_name_gbk, reader.Get("app", "name", "").c_str());
	strcpy(c.in_name, utils::gbk_to_utf8(c.in_name_gbk).c_str());
}

void Exportbin(std::filesystem::path export_bin_path)
{
	const auto& c = bin_config;
	size_t i = 0;
	unsigned char header[128] = { 0 };
	memset(header, 0xFF, 128);

	if (c.in_size == 0)
		return;

	memcpy(header + i, c.identify, 8);				i += 8;
	header[i] = (uint8_t)strlen(c.chip_code);		i += 1;
	memcpy(header + i, c.chip_code, header[i - 1]);	i += 15;
	header[i] = (uint8_t)strlen(c.proj_code);		i += 1;
	memcpy(header + i, c.proj_code, header[i - 1]); i += 23;
	memcpy(header + i, c.hard_version, 6);			i += 6;
	memcpy(header + i, c.soft_version, 6);			i += 6;
	//==============================================================
	header[i] = c.check_type;
	if (c.check_type == CHECK_TYPE_CRC)
	{
		uint16_t crc16 = utils::crc16_modbus(bin_config.in_data.data(), (uint32_t)bin_config.in_data.size());
		header[i + 1] = 2;
		header[i + 2] = crc16 & 0xFF;
		header[i + 3] = crc16 >> 8;
	}
	else if (c.check_type == CHECK_TYPE_SUM)
	{
		uint16_t sum = utils::sum_16(bin_config.in_data.data(), (uint32_t)bin_config.in_data.size());
		header[i + 1] = 2;
		header[i + 2] = sum & 0xFF;
		header[i + 3] = sum >> 8;
	}
	i += 6;
	//==============================================================
	header[i++] = c.block_size & 0xFF;
	header[i++] = c.block_size >> 8;
	header[i++] = c.block_num & 0xFF;
	header[i++] = c.block_num >> 8;
	//==============================================================
	header[i++] = c.upgrade_type;
	//==============================================================
	header[i++] = c.encryption_enable;
	if (c.encryption_enable) 
	{
		header[i++] = c.encryption_type;
		header[i++] = c.encryption_keylen;

		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
		{
			memcpy(header + i, c.enc_xor, 16);
			i += 32;
		}
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
		{
			uint8_t tmp_enc_enc_key[16];
			memcpy(tmp_enc_enc_key, c.enc_key, 16);
			uint8_t tmp_enc_enc_iv[16];
			memcpy(tmp_enc_enc_iv, c.enc_iv, 16);

			utils::aes128_cbc_encrypt(c.enc_key, c.enc_iv, tmp_enc_enc_key, 16);
			utils::aes128_cbc_encrypt(c.enc_key, c.enc_iv, tmp_enc_enc_iv, 16);

			memcpy(header + i, tmp_enc_enc_key, 16);	i += 16;
			memcpy(header + i, tmp_enc_enc_iv, 16);		i += 16;
		}
	}
	else
	{
		i += 34;
	}
	//==============================================================

	std::ofstream out;
	out.open(export_bin_path, std::ios::binary);
	out.write((char*)header, 128);
	if (c.encryption_enable)
	{
		std::vector<uint8_t> enc_buf(c.in_data.size());

		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
		{
			utils::xor_encrypt(c.enc_xor, c.in_data.data(), c.in_data.size(), enc_buf.data());
		}
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
		{
			memcpy(enc_buf.data(), c.in_data.data(), c.in_data.size());
			utils::aes128_cbc_encrypt(c.enc_key, c.enc_iv, enc_buf.data(), enc_buf.size());
		}

		out.write((char*)enc_buf.data(), enc_buf.size());
	}
	else
	{
		out.write((char*)c.in_data.data(), c.in_data.size());
	}
	out.close();
}

static void log16(const char* prefix, uint8_t* data)
{
	logger->AddLog("%s[%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x]\n", prefix, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
}

void Alsbin(std::filesystem::path bin_path)
{
	uint64_t size = std::filesystem::file_size(bin_path);

	std::vector<uint8_t> out_bin(size);
	utils::readFileData(bin_path, out_bin.data());

	uint8_t* header = out_bin.data();
	uint8_t* bin = header + 128;

	size_t bin_size = out_bin.size() - 128;
	std::vector<uint8_t> decrypted_bin(bin_size);

	uint8_t enc_enable = header[71];
	uint8_t enc_type = header[72];
	if (enc_enable)
	{
		if (enc_type == BIN_ENCRYPTION_TYPE_XOR)
		{
			uint8_t* enc_xor = header + 74;
			log16("[Decryption] xor vector:", enc_xor);

			utils::xor_decrypt(enc_xor, bin, bin_size, decrypted_bin.data());
		}
		else if (enc_type == BIN_ENCRYPTION_TYPE_AES)
		{
			memcpy(decrypted_bin.data(), bin, bin_size);

			uint8_t enc_key[16];
			uint8_t* enc_enc_key = header + 74;
			memcpy(enc_key, enc_enc_key, 16);
			log16("[Decryption] aes key underyct:", enc_enc_key);
			utils::aes128_cbc_decrypt(bin_config.enc_key, bin_config.enc_iv, enc_key, 16);
			log16("[Decryption] aes key deryct:", enc_key);

			uint8_t enc_iv[16];
			uint8_t* enc_enc_iv = header + 90;
			memcpy(enc_iv, enc_enc_iv, 16);
			log16("[Decryption] aes iv underyct:", enc_enc_iv);
			utils::aes128_cbc_decrypt(bin_config.enc_key, bin_config.enc_iv, enc_iv, 16);
			log16("[Decryption] aes iv deryct:", enc_iv);

			utils::aes128_cbc_decrypt(enc_key, enc_iv, decrypted_bin.data(), bin_size);
			log16("[Decryption] bin decrypt first 16bytes:", decrypted_bin.data());
		}

		if (memcmp(bin_config.in_data.data(), decrypted_bin.data(), decrypted_bin.size()) == 0)
		{
			logger->AddLog("[Decryption] Success!\n");
		}
	}
	else
	{
		memcpy(decrypted_bin.data(), bin, bin_size);
	}

	uint8_t check_type = header[60];
	uint8_t check_len = header[61];
	uint8_t* check_value = header + 62;
	if (check_type == CHECK_TYPE_CRC) 
	{
		uint16_t crc = utils::crc16_modbus(decrypted_bin.data(), (uint32_t)decrypted_bin.size());
		logger->AddLog("[CRC] %04X\n", crc);
		//crc = (crc >> 8) | (crc << 8);
		if (memcmp(&crc, check_value, check_len) == 0)
		{
			logger->AddLog("[CRC] Success!\n");
		}
	}
	else if (check_type == CHECK_TYPE_SUM)
	{
		uint16_t sum = utils::sum_16(decrypted_bin.data(), (uint32_t)decrypted_bin.size());
		logger->AddLog("[Check SUM] %04X\n", sum);
		//sum = (sum >> 8) | (sum << 8);
		if (memcmp(&sum, check_value, check_len) == 0)
		{
			logger->AddLog("[Check SUM] Success!\n");
		}
	}
}