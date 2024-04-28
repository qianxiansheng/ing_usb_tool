#include "bin.h"

#include <iostream>
#include <fstream>
#include <sstream>
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

	out << "[style]" << std::endl;
	out << "icon_name=" << c.icon_name_gbk << std::endl;
	out << "color0=" << c.color0.colVO.x << "," << c.color0.colVO.y << "," << c.color0.colVO.z << "," << c.color0.colVO.w << std::endl;
	out << "color1=" << c.color1.colVO.x << "," << c.color1.colVO.y << "," << c.color1.colVO.z << "," << c.color1.colVO.w << std::endl;
	out << "color2=" << c.color2.colVO.x << "," << c.color2.colVO.y << "," << c.color2.colVO.z << "," << c.color2.colVO.w << std::endl;
	out << "color3=" << c.color3.colVO.x << "," << c.color3.colVO.y << "," << c.color3.colVO.z << "," << c.color3.colVO.w << std::endl;
	out << "color4=" << c.color4.colVO.x << "," << c.color4.colVO.y << "," << c.color4.colVO.z << "," << c.color4.colVO.w << std::endl;
	out << "color5=" << c.color5.colVO.x << "," << c.color5.colVO.y << "," << c.color5.colVO.z << "," << c.color5.colVO.w << std::endl;
	out << "color6=" << c.color6.colVO.x << "," << c.color6.colVO.y << "," << c.color6.colVO.z << "," << c.color6.colVO.w << std::endl;
	out << "color7=" << c.color7.colVO.x << "," << c.color7.colVO.y << "," << c.color7.colVO.z << "," << c.color7.colVO.w << std::endl;
	out << "color8=" << c.color8.colVO.x << "," << c.color8.colVO.y << "," << c.color8.colVO.z << "," << c.color8.colVO.w << std::endl;
	out << "color9=" << c.color9.colVO.x << "," << c.color9.colVO.y << "," << c.color9.colVO.z << "," << c.color9.colVO.w << std::endl;
	out << std::endl;

	out << "[app]" << std::endl;
	out << "name=" << c.app.name_gbk << std::endl;
	out << "load_address=" << c.app.load_address << std::endl;

	out << "[platform]" << std::endl;
	out << "name=" << c.platform.name_gbk << std::endl;
	out << "load_address=" << c.platform.load_address << std::endl;
	out << std::endl;

	out.close();
}

ImVec4 loadImVec4FromString(std::string s) 
{
	ImVec4 r;
	std::stringstream ss(s);
	char delimiter = ',';
	std::string token;
	std::getline(ss, token, delimiter);
	r.x = std::stof(token);
	std::getline(ss, token, delimiter);
	r.y = std::stof(token);
	std::getline(ss, token, delimiter);
	r.z = std::stof(token);
	std::getline(ss, token, delimiter);
	r.w = std::stof(token);
	return r;
}

void Loadini(std::filesystem::path ini_path)
{
	bin_config_t& c = bin_config;

	INIReader reader(ini_path.generic_string());
	
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

	strcpy(c.icon_name_gbk, reader.Get("style", "icon_name", "icon.png").c_str());
	strcpy(c.icon_name, utils::gbk_to_utf8(c.icon_name_gbk).c_str());
	c.color0.colVO = c.color0.colVE = loadImVec4FromString(reader.Get("style", "color0", "0,0,0,1"));
	c.color1.colVO = c.color1.colVE = loadImVec4FromString(reader.Get("style", "color1", "0.941176,0.941176,0.941176,1"));
	c.color2.colVO = c.color2.colVE = loadImVec4FromString(reader.Get("style", "color2", "0.258824,0.588235,0.980392,0.4"));
	c.color3.colVO = c.color3.colVE = loadImVec4FromString(reader.Get("style", "color3", "0.258824,0.588235,0.980392,1"));
	c.color4.colVO = c.color4.colVE = loadImVec4FromString(reader.Get("style", "color4", "0.0588235,0.529412,0.980392,1"));
	c.color5.colVO = c.color5.colVE = loadImVec4FromString(reader.Get("style", "color5", "0.901961,0.701961,0,1"));
	c.color6.colVO = c.color6.colVE = loadImVec4FromString(reader.Get("style", "color6", "1,1,1,1"));
	c.color7.colVO = c.color7.colVE = loadImVec4FromString(reader.Get("style", "color7", "1,1,1,1"));
	c.color8.colVO = c.color8.colVE = loadImVec4FromString(reader.Get("style", "color8", "1,1,1,1"));
	c.color9.colVO = c.color9.colVE = loadImVec4FromString(reader.Get("style", "color9", "1,1,1,1"));

	strcpy(c.platform.name_gbk, reader.Get("platform", "name", "").c_str());
	strcpy(c.platform.name, utils::gbk_to_utf8(c.platform.name_gbk).c_str());
	strcpy(c.platform.load_address, reader.Get("platform", "load_address", "02003000").c_str());
	c.platform.load_addr = utils::htoi_32(c.platform.load_address);

	strcpy(c.app.name_gbk, reader.Get("app", "name", "").c_str());
	strcpy(c.app.name, utils::gbk_to_utf8(c.app.name_gbk).c_str());
	strcpy(c.app.load_address, reader.Get("app", "load_address", "0202A000").c_str());
	c.app.load_addr = utils::htoi_32(c.app.load_address);
}

void Exportbin(std::filesystem::path export_bin_path)
{
	const auto& c = bin_config;
	size_t i = 0;
	unsigned char header[128] = { 0 };
	memset(header, 0xFF, 128);

	if ((c.app.size) == 0)
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
		uint16_t crc16 = utils::crc16_modbus(c.out_data.data(), (uint32_t)c.out_data.size());
		header[i + 1] = 2;
		header[i + 2] = crc16 & 0xFF;
		header[i + 3] = crc16 >> 8;
	}
	else if (c.check_type == CHECK_TYPE_SUM)
	{
		uint16_t sum = utils::sum_16(c.out_data.data(), (uint32_t)c.out_data.size());
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
	header[i++] = (c.out_data_load_addr >> 0 ) & 0xFF;
	header[i++] = (c.out_data_load_addr >> 8 ) & 0xFF;
	header[i++] = (c.out_data_load_addr >> 16) & 0xFF;
	header[i++] = (c.out_data_load_addr >> 24) & 0xFF;
	header[i++] = (c.out_data.size() >> 0 ) & 0xFF;
	header[i++] = (c.out_data.size() >> 8 ) & 0xFF;
	header[i++] = (c.out_data.size() >> 16) & 0xFF;
	header[i++] = (c.out_data.size() >> 24) & 0xFF;
	//==============================================================
	i += 12;	//Reverse
	//==============================================================
	uint16_t headerCRC = utils::crc16_modbus(header, (sizeof(header) - sizeof(uint16_t)));
	header[i++] = headerCRC & 0xFF;
	header[i++] = headerCRC >> 8;
	//==============================================================

	std::ofstream out;
	out.open(export_bin_path, std::ios::binary);
	out.write((char*)header, 128);
	if (c.encryption_enable)
	{
		std::vector<uint8_t> enc_buf(c.out_data.size());

		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
		{
			utils::xor_encrypt(c.enc_xor, c.out_data.data(), c.out_data.size(), enc_buf.data());
		}
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
		{
			memcpy(enc_buf.data(), c.out_data.data(), c.out_data.size());
			utils::aes128_cbc_encrypt(c.enc_key, c.enc_iv, enc_buf.data(), enc_buf.size());
		}

		out.write((char*)enc_buf.data(), enc_buf.size());
	}
	else
	{
		out.write((char*)c.out_data.data(), c.out_data.size());
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

		if (memcmp(bin_config.out_data.data(), decrypted_bin.data(), decrypted_bin.size()) == 0)
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