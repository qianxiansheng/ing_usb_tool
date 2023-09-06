#pragma once
#include <iostream>
#include <string>
#include <codecvt>
#include <filesystem>
#include <mutex>

namespace utils {

	void HelpMarker(const char* desc);
	void Alert(bool show, const char* title, const char* text);
	bool Confirm(bool show, const char* title, const char* text);
	void AlertEx(bool* show, const char* title, const char* text);



	void readFileData(std::filesystem::path path, void* out_data);
	std::vector<uint8_t> readFileData(std::filesystem::path& path);
	std::string readFileText(std::filesystem::path path);

	uint8_t htoi_4(const char c);
	uint8_t htoi_8(const char* c);
	uint16_t htoi_16(const char* c);
	uint32_t htoi_32(const char* c);
	void ValidateU8Text(char* text, uint8_t& v);
	void ValidateU16Text(char* text, uint16_t& v);
	void ValidateIntText(char* text, uint32_t& v);
	void ValidateDataText(char* text, uint8_t* data, size_t& size);
	void ValidateHex16ByteText(char* text, uint8_t* data);
	bool ValidateVersion(const char* v);


	std::string dec2hex(uint32_t i);
	void printf_hexdump(uint8_t* data, uint16_t size);
	unsigned char Hex2String(const unsigned char* pSrc, unsigned char* dest, int nL);
	void String2Hex(const char* src, unsigned char* dest, int srcL);

	std::wstring utf8_to_wstr(const std::string& src);
	std::string wstr_to_utf8(const std::wstring& src);
	std::string utf8_to_gbk(const std::string& str);
	std::string gbk_to_utf8(const std::string& str);
	std::wstring gbk_to_wstr(const std::string& str);

	void my_sleep(unsigned long milliseconds);
	struct tm time_to_tm(time_t as_time_t);

	long long get_current_system_time_ms();
	long long get_current_system_time_s();

	uint16_t crc16_modbus(uint8_t* data, uint32_t length);
	uint8_t bcc(uint8_t* data, uint32_t length);
	uint16_t sum_16(uint8_t* p1, uint32_t len);

	void xor_encrypt(const uint8_t* xor_vector, const uint8_t* data, size_t size, uint8_t* outData);
	void xor_decrypt(const uint8_t* xor_vector, const uint8_t* data, size_t size, uint8_t* outData);
	void aes128_cbc_encrypt(const uint8_t* key, const uint8_t* iv, uint8_t* buff, size_t size);
	void aes128_cbc_decrypt(const uint8_t* key, const uint8_t* iv, uint8_t* buff, size_t size);
}
