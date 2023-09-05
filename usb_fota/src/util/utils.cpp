#include "utils.h"
#include <sstream>
#include <vector>
#include <fstream>

#include "imgui.h"
#include "aes.h"

// OS Specific sleep
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void utils::HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void utils::Alert(bool show, const char* title, const char* text)
{
	if (show)
		ImGui::OpenPopup(title);

	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(text);
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

bool utils::Confirm(bool show, const char* title, const char* text)
{
	if (show)
		ImGui::OpenPopup(title);

	// Always center this window when appearing
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	bool result = false;

	if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(text);
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup(); 
			result = true;
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup(); 
		}
		ImGui::EndPopup();
	}
	return result;
}


void utils::readFileData(std::filesystem::path path, void* out_data)
{
	std::cout << path << std::endl;
	std::ifstream inFile;
	inFile.open(path, std::ios::in | std::ios::binary);
	while (inFile.read((char*)out_data, std::filesystem::file_size(path)));
	inFile.close();
}

std::vector<uint8_t> utils::readFileData(std::filesystem::path& path)
{
	uint32_t size = std::filesystem::file_size(path);
	std::vector<uint8_t> file(size);
	readFileData(path, file.data());
	return file;
}

std::string utils::readFileText(std::filesystem::path path)
{
	std::ifstream inFile;
	std::stringstream ss;
	std::string content;
	inFile.open(path, std::ios::in);

	while (!inFile.eof())
	{
		inFile >> content;
		ss << content;
	}
	inFile.close();

	return ss.str();
}


uint8_t utils::htoi_4(const char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	return 0;
}
uint8_t utils::htoi_8(const char* c)
{
	return (htoi_4(c[0]) << 4) | htoi_4(c[1]);
}
uint16_t utils::htoi_16(const char* c)
{
	return (htoi_8(c) << 8) | htoi_8(c + 2);
}
uint32_t utils::htoi_32(const char* c)
{
	return (htoi_8(c) << 24) | (htoi_8(c + 2) << 16) | (htoi_8(c + 4) << 8) | htoi_8(c + 6);
}

void utils::ValidateU8Text(char* text, uint8_t& v)
{
	// Load address
	if (text[0] == '\0') {
		v = 0;
		strcpy(text, "00");
	}
	else {
		if (strlen(text) == 1) {
			text[2] = '\0';
			text[1] = text[0];
			text[0] = '0';
		}
		v = utils::htoi_8(text);
	}
}
void utils::ValidateU16Text(char* text, uint16_t& v)
{
	// Load address
	if (text[0] == '\0') {
		v = 0;
		strcpy(text, "0000");
	}
	else {
		if (strlen(text) == 1) {
			text[4] = '\0';
			text[3] = text[0];
			text[2] = '0';
			text[1] = '0';
			text[0] = '0';
		}
		if (strlen(text) == 2) {
			text[4] = '\0';
			text[3] = text[1];
			text[2] = text[0];
			text[1] = '0';
			text[0] = '0';
		}
		if (strlen(text) == 3) {
			text[4] = '\0';
			text[3] = text[2];
			text[2] = text[1];
			text[1] = text[0];
			text[0] = '0';
		}
		v = utils::htoi_16(text);
	}
}
void utils::ValidateIntText(char* text, uint32_t& v)
{
	// Load address
	if (text[0] == '\0') {
		v = 0;
		strcpy(text, "0");
	}
	else {
		v = std::stoi(text);
	}
}
void utils::ValidateDataText(char* text, uint8_t* data, size_t& size)
{
	// Load address
	if (text[0] == '\0') {
		size = 0;
	}
	else {
		if (text[0] == '0' && text[1] == 'x') {
			size = (strlen(text) - 2) / 2;

			for (size_t i = 0; i < size; ++i) {
				data[i] = utils::htoi_8(text + 2 + (i * 2));
			}
		}
		else {
			size = strlen(text);
			strcpy((char*)data, text);
		}
	}
}
void utils::ValidateHex16ByteText(char* text, uint8_t* data)
{
	size_t cl = strlen(text);
	uint8_t rl = 32 - cl;
	std::stringstream ss;
	for (uint8_t i = 0; i < rl; ++i)
		ss << '0';
	ss << text;
	strcpy(text, ss.str().c_str());

	utils::String2Hex(text, data, 32);
}
bool utils::ValidateVersion(const char* v)
{
	if (v[0] != 'V' || v[2] != '.' || v[4] != '.')
		return false;
	if (v[1] < '0' || '9' < v[1])
		return false;
	if (v[3] < '0' || '9' < v[3])
		return false;
	if (v[5] < '0' || '9' < v[5])
		return false;
	return true;
}


std::string utils::dec2hex(uint32_t i)
{
	std::ostringstream ss;
	ss << "0x" << std::hex << i;
	return ss.str();
}

void utils::printf_hexdump(uint8_t* data, uint16_t size)
{
	std::cout << std::hex;
	for (uint16_t i = 0; i < size; ++i) {
		std::cout << (int)data[i];
	}
	std::cout << std::dec << std::endl;
}

unsigned char utils::Hex2String(const unsigned char* pSrc, unsigned char* dest, int nL)
{
	unsigned char* buf_hex = 0;
	int i;
	unsigned char hb;
	unsigned char lb;
	buf_hex = dest;
	memset((char*)buf_hex, 0, sizeof(buf_hex));
	hb = 0;
	lb = 0;
	for (i = 0; i < nL; i++)
	{
		hb = (pSrc[i] & 0xf0) >> 4;
		if (hb >= 0 && hb <= 9)
			hb += 0x30;
		else if (hb >= 10 && hb <= 15)
			hb = hb - 10 + 'A';
		else
			return 1;
		lb = pSrc[i] & 0x0f;
		if (lb >= 0 && lb <= 9)
			lb += 0x30;
		else if (lb >= 10 && lb <= 15)
			lb = lb - 10 + 'A';
		else
			return 1;
		buf_hex[i * 2] = hb;
		buf_hex[i * 2 + 1] = lb;
	}
	return 0;
}

void utils::String2Hex(const char* src, unsigned char* dest, int srcL)
{
	int halfSrcL = srcL / 2;
	for (int i = 0; i < halfSrcL; ++i)
		dest[i] = htoi_8(src + (i * 2));
}


std::wstring utils::utf8_to_wstr(const std::string& src)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(src);
}
std::string utils::wstr_to_utf8(const std::wstring& src)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(src);
}
std::string utils::utf8_to_gbk(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
	std::wstring tmp_wstr = conv.from_bytes(str);

	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	return convert.to_bytes(tmp_wstr);
}
std::string utils::gbk_to_utf8(const std::string& str)
{
	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	std::wstring tmp_wstr = convert.from_bytes(str);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> cv2;
	return cv2.to_bytes(tmp_wstr);
}
std::wstring utils::gbk_to_wstr(const std::string& str)
{
	//GBK locale name in windows
	const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
	return convert.from_bytes(str);
}

void utils::my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
	Sleep(milliseconds); // 100 ms
#else
	usleep(milliseconds * 1000); // 100 ms
#endif
}

struct tm utils::time_to_tm(time_t as_time_t)
{
	struct tm tm;
#if defined(WIN32) || defined(_WINDLL)
	localtime_s(&tm, &as_time_t);  //win api，线程安全，而std::localtime线程不安全
#else
	localtime_r(&as_time_t, &tm);//linux api，线程安全
#endif
	return tm;
}

long long utils::get_current_system_time_ms()
{
	std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count();
}
long long utils::get_current_system_time_s()
{
	std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_since_epoch = time_point_now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch).count();
}

uint16_t utils::crc16_modbus(uint8_t* data, uint16_t length)
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
	return (crc << 8) | (crc >> 8);
}

uint16_t utils::crc16_x25(uint8_t* data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8408;        // 0x8408 = reverse 0x1021
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;                // crc^Xorout
}

uint8_t utils::bcc(uint8_t* data, uint16_t length)
{
	uint8_t bcc = 0;
	while (length--)
		bcc ^= *data++;
	return bcc;
}

uint16_t utils::sum_16(uint8_t* p1, uint32_t len)
{
	uint16_t bcc = 0;
	for (; len > 0; len--)
	{
		bcc += *p1++;
	}
	return bcc;
}

// The data length needs to be an integer multiple of 16
void utils::xor_encrypt(const uint8_t* xor_vector, const uint8_t* data, size_t size, uint8_t* outData)
{
	if (size % 16 != 0) return;
	uint8_t i = 0;
	for (; size > 0; size--)
	{
		*outData++ = xor_vector[i] ^ *data++;

		if (++i == 16) i = 0;
	}
}
// The data length needs to be an integer multiple of 16
void utils::xor_decrypt(const uint8_t* xor_vector, const uint8_t* data, size_t size, uint8_t* outData)
{
	xor_encrypt(xor_vector, data, size, outData);
}

// The data length needs to be an integer multiple of 16
// The encrypted data will replace the original data
void utils::aes128_cbc_encrypt(const uint8_t* key, const uint8_t* iv, uint8_t* buff, size_t size)
{
	if (size % 16 != 0) return;

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_encrypt_buffer(&ctx, buff, size);
}

// The data length needs to be an integer multiple of 16
// The encrypted data will replace the original data
void utils::aes128_cbc_decrypt(const uint8_t* key, const uint8_t* iv, uint8_t* buff, size_t size)
{
	if (size % 16 != 0) return;

	AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);
	AES_CBC_decrypt_buffer(&ctx, buff, size);
}

