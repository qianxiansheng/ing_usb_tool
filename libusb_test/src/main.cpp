#include <stdio.h>
#include <iostream>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <filesystem>

#include "crcLib.h"
#include "aes.h"

static void printf_hexdump(uint8_t* data, uint16_t size)
{
	std::cout << std::hex;
	for (uint16_t i = 0; i < size; ++i) {
		std::cout << (int)data[i];
	}
	std::cout << std::dec << std::endl;
}

uint16_t crc16_modbus2(uint8_t* data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

int main0(int argc, char* argv[])
{
	uint8_t data[] = { 
		0x11, 0x22, 0x33, 0x44, 0x44, 0x44, 0x44, 0x45, 
		0x44, 0x44, 0x45, 0x63, 0x46, 0x34, 0x56, 0x34, 
		0x3D, 0xDD, 0xDE, 0xDD, 0xEC, 0xF4, 0x00, 0x22, 
		0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 
	};

	uint16_t crc = crc16_modbus2(data, 32);

	std::cout << std::hex << (int)crc << std::dec << std::endl;

	std::ofstream ofs;
	ofs.open("crctest.bin", std::ios::binary | std::ios::out);
	ofs.write((char*)data, 32);
	ofs.close();


	return 0;
}

#define AES_BLOCK_LEN 16

void aes_cbc_pcsk7_encrypt(const uint8_t* key, const uint8_t* iv, uint8_t* data, size_t size, uint8_t* outData, size_t* outSize)
{
	size_t l = AES_BLOCK_LEN - size;
	if (l == 0) l = AES_BLOCK_LEN;
	size_t s = size + l;
	uint8_t* buf = (uint8_t*)malloc(s + 100);
	if (buf == NULL)
		return;
	memcpy(buf, data, size);
	memset(buf + size, l, l);

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_encrypt_buffer(&ctx, buf, s);

	memcpy(outData, buf, s);
	*outSize = s;

	free(buf);
}
void aes_cbc_pcsk7_decrypt(const uint8_t* key, const uint8_t* iv, uint8_t* data, size_t size, uint8_t* outData, size_t* outSize)
{
	uint8_t* buf = (uint8_t*)malloc(size + 100);
	if (buf == NULL)
		return;
	memcpy(buf, data, size);

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_decrypt_buffer(&ctx, buf, size);

	int n = buf[size - 1];
	int s = size - n;
	memcpy(outData, buf, s);
	*outSize = s;

	free(buf);
}

int main1()
{
	const uint8_t key[17] = "HefSB4whqSG5Ly9L";
	const uint8_t iv[17] = "apNgbnsyy6bAXnNm";
	uint8_t data[16] = "Hello World!";

	uint8_t encrypt_buf[16] = { 0 };
	size_t encrypt_size = 0;

	uint8_t decrypt_buf[16] = { 0 };
	size_t decrypt_size = 0;

	aes_cbc_pcsk7_encrypt(key, iv, data, 12, encrypt_buf, &encrypt_size);
	
	for (uint8_t i = 0; i < encrypt_size; ++i)
	{
		std::cout << std::hex << (int)encrypt_buf[i] << " ";
	}
	std::cout << std::endl;

	aes_cbc_pcsk7_decrypt(key, iv, encrypt_buf, encrypt_size, decrypt_buf, &decrypt_size);
	
	std::cout << "Over" << std::endl;

	return 0;
}


#include <stdio.h>

union {
	uint16_t s;
	uint8_t c[sizeof(uint16_t)];
}un;

int main2()
{
	uint8_t data[] = { 0x11, 0x22, 0x33, 0x44, 0x44, 0x44, 0x44, 0x45, 0x44, 0x44, 0x45, 0x63, 0x46, 0x34, 0x56, 0x34, 0x3D, 0xDD, 0xDE, 0xDD, 0xEC, 0xF4, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 };

	uint16_t r = crc16_modbus(data, sizeof(data));

	uint8_t lsb = r;
	uint8_t msb = r >> 8;

	printf("%02X %02X\n", lsb, msb);

	uint16_t c = 0x8572;
	uint8_t* pc = (uint8_t*)&c;
	printf("%02X %02X\n", pc[0], pc[1]);

	un.s = 0x8572;
	printf("%02X %02X\n", un.c[0], un.c[1]);

	return 0;
}

void readFileData(std::filesystem::path path, void* out_data)
{
	std::cout << path << std::endl;
	std::ifstream inFile;
	inFile.open(path, std::ios::in | std::ios::binary);
	while (inFile.read((char*)out_data, std::filesystem::file_size(path)));
	inFile.close();
}

std::vector<uint8_t> readFileData(std::filesystem::path& path)
{
	uint32_t size = std::filesystem::file_size(path);
	std::vector<uint8_t> file(size);
	readFileData(path, file.data());
	return file;
}

int main()
{
	std::filesystem::path filename("D:\\myResource\\work\\c++\\ing_usb_fota\\bin\\x64\\Release\\iap_template.exe");

	std::vector<uint8_t> data = readFileData(filename);

	std::ofstream ofs;

	ofs.open("D:\\myResource\\work\\c++\\ing_usb_fota\\ing_usb_fota\\usb_fota\\src\\bingen\\template.cpp");

	ofs << "const uint8_t template_data[] = {" << std::endl;

	char buf[] = "0xFF, ";

	uint32_t lineSize = 16;
	uint32_t lineNum  = (data.size() - 1) / lineSize + 1;
	uint32_t lineLast = (data.size() - 1) % lineSize + 1;
	for (uint32_t i = 0; i < lineNum; ++i)
	{
		if (i == lineNum - 1)
			lineSize = lineLast;

		for (uint32_t j = 0; j < lineSize; ++j)
		{
			sprintf(buf, "0x%02X, ", data[i * 16 + j]);

			ofs << buf;
		}
		ofs << std::endl;
	}

	ofs << "};" << std::endl;

	ofs.flush();
	ofs.close();
}