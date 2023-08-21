#include "pch.h"

#include <iostream>
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <sstream>

#include "imgui_impl_win32.h"

#include "winbase.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "libusb-1.0/libusb.h"

#include "util/thread_pool.h"
#include "util/utils.h"
#include "usb/usb_utils.h"
#include "bin.h"

#include "imgui/extensions/ImConsole.h"
#include "imgui/extensions/ImFileDialog.h"
#include "imgui/extensions/imgui_memory_editor.h"


#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;
static bool opt_fullscreen = true;
static bool opt_showdemowindow = false;
static bool opt_showlogwindow = true;
static bool opt_showusbdevicetreewindow = false;
static bool opt_showusbsendwindow = false;
static bool opt_showbingenwindow = true;
static bool opt_showbinviewerwindow = true;

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

static std::filesystem::path work_dir = std::filesystem::current_path();
static std::filesystem::path ini_path = std::filesystem::current_path().concat("\\bin_config.ini");
static std::filesystem::path export_path;
static std::filesystem::path export_dir;


ImLogger* logger;
static uint8_t* p_mem = NULL;
static size_t s_mem;
static MemoryEditor mem_edit;


uint16_t vid = 0;
uint16_t pid = 0;
uint32_t interface = 0;
uint8_t epIn = 0;
uint8_t epOut = 0;
uint32_t read_size = 0;
uint32_t write_size = 0;
size_t data_size = 0;
uint8_t sendData[DATA_INPUT_BUFF_MAX_SIZE] = { 0 };
uint8_t recvData[DATA_INPUT_BUFF_MAX_SIZE] = { 0 };

char vidText[5] = "FFF1";
char pidText[5] = "FA2F";
char interfaceText[5] = "0";
char epOutText[3] = "02";
char epInText[3] = "81";
char writeSizeText[32] = "64";
char readSizeText[32] = "64";
char sendText[DATA_INPUT_BUFF_MAX_SIZE] = "Ingchips 123";
char recvText[DATA_INPUT_BUFF_MAX_SIZE] = "";
std::unordered_map<uint32_t, usb::device> deviceMap;


extern void* CreateTexture(uint8_t* data, int w, int h, char fmt);
extern void DeleteTexture(void* tex);

extern bin_config_t bin_config;

static void ImGuiDCXAxisAlign(float v)
{
	ImGui::SetCursorPos(ImVec2(v, ImGui::GetCursorPos().y));
}

static void ImGuiDCYMargin(float v)
{
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + v));
}


static void Cube_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
	std::cout << "ClearAll" << std::endl;
}

static void* Cube_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
	return &export_dir;
}

static void Cube_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	std::string s(line);

	size_t i = s.find_first_of('=');
	if (i >= s.size()) return;

	auto name = s.substr(0, i);
	auto value = s.substr(i + 1);


	if (name == "export_dir")
	{
		export_dir = value;
	}
}


static void Cube_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
	buf->append("[FOTA][config]\n");
	buf->appendf("export_dir=%s\n", export_dir.generic_string().c_str());
}

static bool main_init(int argc, char* argv[])
{
	// Enable Dock
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Load ini
	Loadini(ini_path);

	// Theme
	ImGui::StyleColorsLight();

	// File dialog adapter
	ifd::FileDialog::Instance().CreateTexture = [](uint8_t* data, int w, int h, char fmt) -> void* {
		return CreateTexture(data, w, h, fmt);
	};
	ifd::FileDialog::Instance().DeleteTexture = [](void* tex) {
		DeleteTexture(tex);
	};

	// Font
	io.Fonts->AddFontFromFileTTF(DEFAULT_FONT, 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	// ImConsole
	logger = new ImLogger();

	//if (opt_showusbdevicetreewindow)
	deviceMap = usb::enumerate_all_device();


	static ImGuiSettingsHandler ini_handler;
	ini_handler.TypeName = "FOTA";
	ini_handler.TypeHash = ImHashStr("FOTA");
	ini_handler.ClearAllFn = Cube_ClearAll;
	ini_handler.ReadOpenFn = Cube_ReadOpen;
	ini_handler.ReadLineFn = Cube_ReadLine;
	ini_handler.ApplyAllFn = NULL;
	ini_handler.WriteAllFn = Cube_WriteAll;
	ImGui::AddSettingsHandler(&ini_handler);

    return true;
}

static void main_shutdown(void)
{
	delete logger;
}


//==================================================================================
void ShowUSBDeviceTreeWindow1(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("USB dev tree", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh Device Tree"))
	{
		deviceMap = usb::enumerate_all_device();
	}


	if (ImGui::TreeNode("My Computer"))
	{
		for (const auto& device : deviceMap)
		{
			if (ImGui::TreeNode(device.second.name.c_str()))
			{
				ImGui::Text(device.second.descriptor_text.c_str());

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}


	ImGui::End();
}


#define USB_TREE_NODE_ATTR_110(name, value)\
ImGui::TableNextRow();\
ImGui::TableNextColumn();\
ImGui::Text(name);\
ImGui::TableNextColumn();\
ImGui::Text(value);\
ImGui::TableNextColumn();\
ImGui::TextDisabled("--");
#define USB_TREE_NODE_ATTR_111(name, value, desc)\
ImGui::TableNextRow();\
ImGui::TableNextColumn();\
ImGui::Text(name);\
ImGui::TableNextColumn();\
ImGui::Text(value);\
ImGui::TableNextColumn();\
ImGui::Text(desc);

void ShowEndpointTreeNode(const usb::endpoint_t& endpoint)
{
	std::stringstream ss;
	ss << "endpoint_" << std::hex << (int)endpoint.bEndpointAddress << 'h' << std::dec;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(ss.str().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("END POINT");
	ImGui::TableNextColumn();
	ImGui::Text((endpoint.bEndpointAddress & 0xF0) ? "IN" : "OUT");

	if (open) {
		ss.str("");
		ss << std::hex << (int)endpoint.bEndpointAddress << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bEndpointAddress", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)endpoint.bmAttributes << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bmAttributes", ss.str().c_str());
		ss.str("");
		ss << (int)endpoint.wMaxPacketSize;
		USB_TREE_NODE_ATTR_110("wMaxPacketSize", ss.str().c_str());

		ImGui::TreePop();
	}
}
void ShowInterfaceTreeNode(const usb::interface_t& _interface)
{
	std::stringstream ss;
	ss << "interface" << (int)_interface.bInterfaceNumber << '[' << (int)_interface.index << ']';

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(ss.str().c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("INTERFACE");
	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	if (open) {

		ss.str("");
		ss << (int)_interface.bInterfaceNumber;
		USB_TREE_NODE_ATTR_110("bInterfaceNumber", ss.str().c_str());
		ss.str("");
		ss << (int)_interface.bAlternateSetting;
		USB_TREE_NODE_ATTR_110("bAlternateSetting", ss.str().c_str());
		ss.str("");
		ss << (int)_interface.bNumEndpoints;
		USB_TREE_NODE_ATTR_110("bNumEndpoints", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceClass << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceClass", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceSubClass << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceSubClass", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)_interface.bInterfaceProtocol << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bInterfaceProtocol", ss.str().c_str());


		for (const auto& endpoint : _interface.endpointList)
			ShowEndpointTreeNode(endpoint);
		ImGui::TreePop();
	}
}
void ShowConfigurationTreeNode(const usb::configuration_t& configuration)
{
	std::stringstream ss;
	ss << "configuration" << (int)configuration.bConfigurationValue;
	std::string configName = ss.str();

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx(configName.c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::TableNextColumn();
	ImGui::Text("CONFIGURATION");
	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	if (open) {
		ss.str("");
		ss << (int)configuration.wTotalLength;
		USB_TREE_NODE_ATTR_110("wTotalLength", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.bConfigurationValue;
		USB_TREE_NODE_ATTR_110("bConfigurationValue", ss.str().c_str());
		ss.str("");
		ss << std::hex << (int)configuration.bmAttributes << 'h' << std::dec;
		USB_TREE_NODE_ATTR_110("bmAttributes", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.bNumInterfaces;
		USB_TREE_NODE_ATTR_110("bNumInterfaces", ss.str().c_str());
		ss.str("");
		ss << (int)configuration.MaxPower;
		USB_TREE_NODE_ATTR_110("MaxPower", ss.str().c_str());

		for (const auto& _interface : configuration.interfaceList)
			ShowInterfaceTreeNode(_interface);
		ImGui::TreePop();
	}
}
void ShowDeviceTreeNode(const usb::device& device)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
	if (device.name == "VIDfff1&PIDfa2f" || device.manufacturer == "Ingchips")
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	bool deviceOpen = ImGui::TreeNodeEx(device.name.c_str(), flags);
	ImGui::TableNextColumn();
	if (device.manufacturer == "") ImGui::TextDisabled("--");
	else                           ImGui::Text(device.manufacturer.c_str());
	ImGui::TableNextColumn();
	if (device.product == "")      ImGui::TextDisabled("--");
	else                           ImGui::Text(device.product.c_str());

	if (deviceOpen)
	{
		for (const auto& configuration : device.configurationList)
			ShowConfigurationTreeNode(configuration);

		ImGui::TreePop();
	}
}
void ShowUSBDeviceTreeWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("USB dev tree", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh Device Tree"))
	{
		deviceMap = usb::enumerate_all_device();
	}


	const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

	if (ImGui::BeginTable("usb_device_tree", 3, flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Desc", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		bool open = ImGui::TreeNodeEx("My Window", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
		ImGui::TableNextColumn();
		ImGui::TextDisabled("--");
		ImGui::TableNextColumn();
		ImGui::TextDisabled("--");
			
		if (open)
		{
			for (const auto& devicePair : deviceMap)
				ShowDeviceTreeNode(devicePair.second);
			ImGui::TreePop();
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

static void ValidateU8Text(char* text, uint8_t& v)
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
static void ValidateU16Text(char* text, uint16_t& v)
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
static void ValidateIntText(char* text, uint32_t& v)
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
static void ValidateDataText(char* text, uint8_t* data, size_t& size)
{
	// Load address
	if (text[0] == '\0') {
		size = 0;
		memset(data, 0, DATA_INPUT_BUFF_MAX_SIZE);
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
static void ValidateHex16ByteText(char* text, uint8_t* data)
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


static void USB_SendData(uint16_t vid, uint16_t pid, int interfaceIndex, int ep, uint8_t* data, size_t size)
{
	libusb_context* temp_ctx;

	// 初使化上下文
	int nRet = libusb_init(&temp_ctx);
	if (nRet < 0)
	{
		throw std::exception((std::stringstream() << "libusb_init(NULL) failed:[" << libusb_strerror(nRet) << "] \n").str().c_str());
	}

	printf("libusb_init(NULL) ok \n");

	// 打开指定厂商的某类产品
	libusb_device_handle* pHandle = libusb_open_device_with_vid_pid(temp_ctx, vid, pid);
	if (pHandle == NULL)
	{
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_open_device_with_vid_pid(0x" << std::hex << (int)vid << ", 0x" << (int)pid << std::dec << ") failed\n").str().c_str());
	}

	logger->AddLog("[info] %s\n", (std::stringstream() << "libusb_open_device_with_vid_pid(0x" << std::hex << (int)vid << ", 0x" << (int)pid << std::dec << ") ok").str().c_str());

	// 声明使用第n个接口
	nRet = libusb_claim_interface(pHandle, interfaceIndex);
	if (nRet < 0)
	{
		libusb_close(pHandle);
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_claim_interface(" << interfaceIndex << ") failed:[" << libusb_strerror(nRet) << "] \n").str().c_str());
	}

	logger->AddLog("libusb_claim_interface(%d) ok \n", interfaceIndex);

	// 向指定端点发送数据
	int nActualBytes = 0;
	nRet = libusb_bulk_transfer(pHandle, ep, (unsigned char*)data, size, &nActualBytes, 1000);
	if (nRet < 0)
	{
		libusb_release_interface(pHandle, interfaceIndex);
		libusb_close(pHandle);
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") write failed:[" << libusb_strerror(nRet) << "] \n").str().c_str());
	}

	logger->AddLog("[info] %s\n", (std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") write size:[" << nActualBytes << "]").str().c_str());

	unsigned char hexbuf[513] = {0};
	utils::Hex2String(data, hexbuf, size);
	logger->AddLog("[info] %s\n", hexbuf);

	// 释放第n个接口
	libusb_release_interface(pHandle, interfaceIndex);

	// 关闭设备
	libusb_close(pHandle);

	// 释放上下文
	libusb_exit(temp_ctx);
}
static int USB_RecvData(uint16_t vid, uint16_t pid, int interfaceIndex, int ep, uint8_t* data, size_t size)
{
	libusb_context* temp_ctx;

	// 初使化上下文
	int nRet = libusb_init(&temp_ctx);
	if (nRet < 0)
	{
		throw std::exception((std::stringstream() << "libusb_init(NULL) failed:[" << libusb_strerror(nRet) << "]").str().c_str());
	}

	logger->AddLog("[info] libusb_init ok\n");

	// 打开指定厂商的某类产品
	libusb_device_handle* pHandle = libusb_open_device_with_vid_pid(temp_ctx, vid, pid);
	if (pHandle == NULL)
	{
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_open_device_with_vid_pid(0x" << std::hex << (int)vid << ", 0x" << (int)pid << std::dec << ") failed").str().c_str());
	}

	logger->AddLog("[info] %s\n", (std::stringstream() << "libusb_open_device_with_vid_pid(0x" << std::hex << (int)vid << ", 0x" << (int)pid << std::dec << ") ok").str().c_str());

	// 声明使用第n个接口
	nRet = libusb_claim_interface(pHandle, interfaceIndex);
	if (nRet < 0)
	{
		libusb_close(pHandle);
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_claim_interface(" << interfaceIndex << ") failed:[" << libusb_strerror(nRet) << "]").str().c_str());
	}

	logger->AddLog("libusb_claim_interface(%d) ok \n", interfaceIndex);

	// 从指定端点接收数据
	int nActualBytes = 0;
	nRet = libusb_bulk_transfer(pHandle, ep, (unsigned char*)data, size, &nActualBytes, 1000);
	if (nRet < 0)
	{
		libusb_release_interface(pHandle, interfaceIndex);
		libusb_close(pHandle);
		libusb_exit(NULL);
		throw std::exception((std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") read failed:[" << libusb_strerror(nRet) << "]").str().c_str());
	}

	logger->AddLog("[info] %s\n", (std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") read size:[" << nActualBytes << "]").str().c_str());

	// 释放第n个接口
	libusb_release_interface(pHandle, interfaceIndex);

	// 关闭设备
	libusb_close(pHandle);

	// 释放上下文
	libusb_exit(temp_ctx);

	return nActualBytes;
}

static void ShowUSBSendWindow(bool* p_open)
{
	bool btnSend = false;
	bool btnRecv = true;
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("USB Send", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	ImGui::Text("VID");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextEx("##VID", NULL, vidText, sizeof(vidText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::Text("PID");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextEx("##PID", NULL, pidText, sizeof(pidText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::Text("INTERFACE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextEx("##INTERFACE", NULL, interfaceText, sizeof(interfaceText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::Text("EP OUT");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextEx("##EP OUT", NULL, epOutText, sizeof(epOutText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::SameLine();
	ImGuiDCXAxisAlign(220); ImGui::Text("WRITE SIZE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(320); ImGui::InputTextEx("##WRITE SIZE", NULL, writeSizeText, sizeof(writeSizeText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::SameLine();
	btnSend = ImGui::Button("Write");
	
	ImGui::Text("EP IN");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextEx("##EP IN", NULL, epInText, sizeof(epInText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::SameLine();
	ImGuiDCXAxisAlign(220); ImGui::Text("READ SIZE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(320); ImGui::InputTextEx("##READ SIZE", NULL, readSizeText, sizeof(readSizeText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::SameLine();
	btnRecv = ImGui::Button("Receive");

	static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
	ImGui::Text("DATA Send");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextMultiline("##DATA SEND", sendText, sizeof(sendText), ImVec2(500.0f, ImGui::GetTextLineHeight() * 5), flags);

	ImGui::Text("DATA Recv");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100);ImGui::InputTextMultiline("##DATA RECV", recvText, sizeof(recvText), ImVec2(500.0f, ImGui::GetTextLineHeight() * 5), flags);
	

	ValidateU16Text(vidText, vid);
	ValidateU16Text(pidText, pid);
	ValidateIntText(interfaceText, interface);
	ValidateU8Text(epOutText, epOut);
	ValidateU8Text(epInText, epIn);
	ValidateIntText(writeSizeText, write_size);
	ValidateIntText(readSizeText, read_size);
	ValidateDataText(sendText, sendData, data_size);

	if (btnSend)
	{
		try
		{
			USB_SendData(vid, pid, interface, epOut, sendData, write_size);
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
			logger->AddLog("[Send] %s\n", e.what());
		}
		memset(sendData, 0, DATA_INPUT_BUFF_MAX_SIZE);
	}
	if (btnRecv)
	{
		memset(recvData, 0, DATA_INPUT_BUFF_MAX_SIZE);
		try
		{
			int len = USB_RecvData(vid, pid, interface, epIn, recvData, read_size);

			logger->AddLog("[actual len] %d\n", len);

			recvText[0] = '0';
			recvText[1] = 'x';

			utils::Hex2String(recvData, (unsigned char*)recvText + 2, len);
		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
			logger->AddLog("[Receive] %s\n", e.what());
		}
	}

	ImGui::End();
}

static bool ValidateVersion(const char* v)
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


static void CalcCrc()
{
	auto& c = bin_config;

	if (c.in_size == 0)
		return ;

	if (c.check_type == CHECK_TYPE_CRC)
	{
		uint16_t crc16 = utils::crc16_modbus(c.in_data.data(), c.in_data.size());
		c.in_crc = crc16;
	}
	else if (c.check_type == CHECK_TYPE_SUM)
	{
		uint16_t sum = utils::sum_16(c.in_data.data(), c.in_data.size());
		c.in_crc = sum;
	}
}
static void ReloadInFile()
{
	int size = ((bin_config.in_size - 1) / 16 + 1) * 16;

	bin_config.in_data.resize(size);
	memset(bin_config.in_data.data(), 0xFF, size);
	utils::readFileData(bin_config.in_name_gbk, bin_config.in_data.data());

	CalcCrc();
}

static std::string export_file_name()
{
	auto& c = bin_config;

	char buf[32] = { 0 };

	std::stringstream ss;
	ss << "INGOTA_";
	if (strlen(c.proj_code) <= 5)
		ss << c.proj_code << '_';
	else
	{
		memcpy(buf, c.proj_code, 5);
		buf[5] = '\0';
		ss << buf << "_";
	}
	ss << "HW" << c.hard_version[1] << "_" << c.hard_version[3] << "_" << c.hard_version[5] << "_";
	ss << "SW" << c.soft_version[1] << "_" << c.soft_version[3] << "_" << c.soft_version[5] << "_";

	if (c.check_type == CHECK_TYPE_CRC)
		ss << "CRC_";
	else if (c.check_type == CHECK_TYPE_SUM)
		ss << "SUM_";

	if (c.upgrade_type == UPGRADE_TYPE_APP_ONLY)
		ss << "A_";
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_APP)
		ss << "PA_";
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT)
		ss << "PB_";
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT_APP)
		ss << "PBA_";

	if (c.encryption_enable)
	{
		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
			ss << "Y0_";
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
			ss << "Y1_";
		else
			ss << "X_";
	}
	else
	{
		ss << "N_";
	}

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
	auto as_time_t = std::chrono::system_clock::to_time_t(t);
	struct tm tm = utils::time_to_tm(as_time_t);

	sprintf(buf, "%04d%02d%02d_%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

	ss << buf;
	ss << ".bin";

	return ss.str();
}

static std::filesystem::path DefaultExportDir()
{
	return std::filesystem::path(work_dir).concat("\\output\\");
}

static void GenerateBin()
{
	export_path = std::filesystem::path(export_dir).concat(export_file_name());

	Exportbin(export_path);

	std::cout << export_path << std::endl;

	uint64_t size = std::filesystem::file_size(export_path);
	bin_config.out_data.resize(size);
	utils::readFileData(export_path, bin_config.out_data.data());

	p_mem = bin_config.out_data.data();
	s_mem = bin_config.out_data.size();

	logger->AddLog("[Export] %s\n", utils::gbk_to_utf8(export_path.generic_string()).c_str());
}

static void AnalysisBin()
{
	Alsbin(export_path);
}



static void ShowSubTitle(const char* title)
{
	ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
	float x = ImGui::CalcTextSize(title).x;
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	ImGuiDCYMargin(10);

	float item_top_y = window->DC.CursorPos.y;
	ImGui::Text(title);
	float item_bottom_y = window->DC.CursorPos.y;
	float item_y_half = (item_bottom_y - item_top_y) / 2;

	window->DrawList->AddRectFilled(
		ImVec2(window->ContentRegionRect.Min.x + x + 5, item_top_y + item_y_half - 0.5f),
		ImVec2(window->ContentRegionRect.Min.x + contentRegionAvail.x, item_bottom_y - item_y_half + 0.5f),
		0xFFB6B6B6
	);

	ImGuiDCYMargin(10);
}

static void ShowBinGenWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("Bin Editor", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	bool show_alert_export_success = false;
	bool show_alert_analysis_success = false;

	const int cl = 200;
	char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
	char buf[7] = { 0 };
	auto& c = bin_config;

	struct combo_item
	{
		const char name[32];
		int value;
	};

	const combo_item check_type_items[] = {
		{"CRC", CHECK_TYPE_CRC},
		{"SUM", CHECK_TYPE_SUM}
	};

	const combo_item upgrade_type_items[] = { 
		{"APP ONLY", UPGRADE_TYPE_APP_ONLY}, 
		{"PLATFORM + APP", UPGRADE_TYPE_PLATFORM_APP},
		{"PLATFORM + BOOT", UPGRADE_TYPE_PLATFORM_BOOT},
		{"PLATFORM + BOOT + APP", UPGRADE_TYPE_PLATFORM_BOOT_APP}
	};

	const combo_item encryption_type_items[] = {
		{"XOR", BIN_ENCRYPTION_TYPE_XOR},
		{"AES", BIN_ENCRYPTION_TYPE_AES}
	};

	static int check_type_idx = 0;
	static int upgrade_type_idx = 0;
	static int encryption_type_idx = 0;

	if (c.check_type == CHECK_TYPE_CRC)							check_type_idx = 0;
	else if (c.check_type == CHECK_TYPE_SUM)					check_type_idx = 1;
	else														check_type_idx = 0;

	if (c.upgrade_type == UPGRADE_TYPE_APP_ONLY)				upgrade_type_idx = 0;
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_APP)		upgrade_type_idx = 1;
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT)		upgrade_type_idx = 2;
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT_APP)	upgrade_type_idx = 3;
	else														upgrade_type_idx = 0;

	if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)			encryption_type_idx = 0;
	else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)		encryption_type_idx = 1;
	else														encryption_type_idx = 1;

	const char* check_type_value = check_type_items[check_type_idx].name;
	const char* upgrade_type_value = upgrade_type_items[upgrade_type_idx].name;
	const char* encryption_type_value = encryption_type_items[encryption_type_idx].name;

	ShowSubTitle("Base");//=============================================================

	ImGui::Text("Chip code");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##CHIP_CODE", "", c.chip_code, sizeof(c.chip_code), ImVec2(300.0f, 0.0f), 0);
	ImGui::Text("Project code");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##PROJ_CODE", "", c.proj_code, sizeof(c.proj_code), ImVec2(300.0f, 0.0f), 0);
	strcpy(buf, c.hard_version);
	ImGui::Text("Hardware version");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##HARD_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
	if (ValidateVersion(buf))	strcpy(c.hard_version, buf);
	strcpy(buf, c.soft_version);
	ImGui::SameLine();
	utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");
	ImGui::Text("Software version");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##SOFT_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
	if (ValidateVersion(buf))	strcpy(c.soft_version, buf);
	ImGui::SameLine();
	utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");

	ImGui::Text("Block Size");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputScalar("##BLOCK_SIZE", ImGuiDataType_U16, &c.block_size, NULL, "%u");
	if (c.block_size < 12) c.block_size = 12;
	if (c.block_size > 8192) c.block_size = 8192;
	if (c.in_size == 0) c.block_num = 0;
	else c.block_num = (c.in_size - 1) / c.block_size + 1;
	ImGui::SameLine();
	utils::HelpMarker("Range: between 12 and 8192 bytes.");

	ImGui::Text("Upgrate Type");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	if (ImGui::BeginCombo("##UPGRADE_TYPE", upgrade_type_value, 0))
	{
		for (int n = 0; n < IM_ARRAYSIZE(upgrade_type_items); n++)
		{
			const bool is_selected = (upgrade_type_idx == n);
			if (ImGui::Selectable(upgrade_type_items[n].name, is_selected)) {
				upgrade_type_idx = n;
				c.upgrade_type = upgrade_type_items[n].value;
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ShowSubTitle("Check");//============================================================

	ImGui::Text("Check Type");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	if (ImGui::BeginCombo("##CHECK_TYPE", check_type_value, 0))
	{
		for (int n = 0; n < IM_ARRAYSIZE(check_type_items); n++)
		{
			const bool is_selected = (check_type_idx == n);
			if (ImGui::Selectable(check_type_items[n].name, is_selected)) {
				check_type_idx = n;
				c.check_type = check_type_items[n].value;

				CalcCrc();
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Text("[%08X]", c.in_crc);
	ImGui::SameLine();
	utils::HelpMarker("Used for verifying the original FOTA bin.");

	ShowSubTitle("Encryption");//=======================================================


	ImGui::Text("Encryption enable");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::Checkbox("##ENCRYPTION_ENABLE", &c.encryption_enable);
	ImGui::SameLine();
	utils::HelpMarker("Encrypt FOTA bin data, programmers know the key, old programs need to know how to decrypt it.");
	

	if (c.encryption_enable)
	{
		ImGui::Text("Encryption Type");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		if (ImGui::BeginCombo("##ENCRYPTION_TYPE", encryption_type_value, 0))
		{
			for (int n = 0; n < IM_ARRAYSIZE(encryption_type_items); n++)
			{
				const bool is_selected = (encryption_type_idx == n);
				if (ImGui::Selectable(encryption_type_items[n].name, is_selected)) {
					encryption_type_idx = n;
					c.encryption_type = encryption_type_items[n].value;
				}
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	
		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
		{
			c.encryption_keylen = 16;

			ImGui::Text("XOR Vector");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::InputTextEx("##SOR Vector", "", c.encryption_xor, sizeof(c.encryption_xor), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			ImGui::Text("hex");
			ValidateHex16ByteText(c.encryption_xor, c.enc_xor);
		} 
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES) 
		{
			c.encryption_keylen = 16;

			ImGui::Text("Encryption Key");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::InputTextEx("##ENCRYPTION_KEY", "", c.encryption_key, sizeof(c.encryption_key), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			ImGui::Text("hex");
			ValidateHex16ByteText(c.encryption_key, c.enc_key);

			ImGui::Text("Encryption iv");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::InputTextEx("##ENCRYPTION_IV", "", c.encryption_iv, sizeof(c.encryption_iv), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			ImGui::Text("hex");
			ValidateHex16ByteText(c.encryption_iv, c.enc_iv);
		}
	}

	ShowSubTitle("Choose Bin");//======================================================

	strcpy(bin, c.in_name);
	//=================================================================================
	// Input File Path
	//=================================================================================
	ImGui::Text("Bin");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##BIN_FILE_NAME", "", c.in_name, sizeof(c.in_name), ImVec2(600, 0.0f), 0);
	ImGui::SameLine();


	//=================================================================================
	// Tip
	//=================================================================================
	if (c.in_name[0] != '\0')
	{
		ImGui::SameLine();
		if (c.in_size == 0) {
			ImGui::Text("Error: file not exists");
		}
		else {
			ImGui::Text("OK. file size = %dB (padding:%dB)", c.in_size, c.in_data.size());
		}
	}
	//=================================================================================
	// File Dailog
	//=================================================================================
	if (ifd::FileDialog::Instance().IsDone("BIN_OPEN")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			if (res.size() > 0) {
				std::filesystem::path path = res[0];
				std::string pathStr = path.u8string();
				strcpy(c.in_name, pathStr.c_str());
			}
		}
		ifd::FileDialog::Instance().Close();
	}

	strcpy(c.in_name_gbk, utils::utf8_to_gbk(c.in_name).c_str());
	//=================================================================================
	// Get File Size
	//=================================================================================
	auto path = c.in_name_gbk;
	c.in_size = std::filesystem::is_regular_file(path) ?
		static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;

	//=================================================================================
	// Reload File Binary Data
	//=================================================================================
	if (c.in_size != 0 && (c.in_data.size() == 0 || strcmp(bin, c.in_name) != 0))
		ReloadInFile();

	ImGui::Text("Export Directory");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::Text(utils::gbk_to_utf8(export_dir.generic_string()).c_str());

	//=================================================================================
	// Bin Select
	//=================================================================================
	if (ImGui::Button("Import Bin"))
		ifd::FileDialog::Instance().Open("BIN_OPEN", "Choose bin", "bin {.bin},.*", false);

	if (c.in_size == 0)
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Export bin"))
	{
		if (!std::filesystem::exists(export_dir))
			export_dir = DefaultExportDir();

		GenerateBin();
		show_alert_export_success = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Export to specified directory"))
	{
		std::cout << export_dir.generic_string() << std::endl;
		ifd::FileDialog::Instance().Open("ExportBinFile", "Choose export directory", "", false, utils::gbk_to_utf8(export_dir.generic_string()));
	}
	if (c.in_size == 0)
		ImGui::EndDisabled();

	if (!std::filesystem::exists(export_path))
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Analysis bin"))
	{
		AnalysisBin();
		show_alert_analysis_success = true;
	}
	if (!std::filesystem::exists(export_path))
		ImGui::EndDisabled();

	if (ifd::FileDialog::Instance().IsDone("ExportBinFile")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			std::string res = utils::utf8_to_gbk(ifd::FileDialog::Instance().GetResult().u8string());
			export_dir = std::filesystem::path(res);

			GenerateBin();

			show_alert_analysis_success = true;
		}
		ifd::FileDialog::Instance().Close();
	}
	
	utils::Alert(show_alert_export_success, "Message##m1", "Export bin finished.");
	utils::Alert(show_alert_analysis_success, "Message##m2", "Analysis bin finished.");


	ImGui::End();
}

static void ShowLog(bool* p_open)
{
	logger->Draw("logger", p_open);
}

static void ShowBinViewer(bool* p_open)
{
	mem_edit.DrawWindow("Binary Viewer", p_mem, s_mem);
}

static void ShowRootWindowMenu()
{
	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			if (ImGui::MenuItem("Open ini")) {
			}

			if (ImGui::MenuItem("Save", "Ctrl + S")) {
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Option")) {

			ImGui::MenuItem("Demo Window", NULL, &opt_showdemowindow);
			ImGui::MenuItem("Log Window", NULL, &opt_showlogwindow);
			ImGui::MenuItem("Binary Viewer", NULL, &opt_showbinviewerwindow);
			ImGui::Separator();
			ImGui::MenuItem("USB Device tree", NULL, &opt_showusbdevicetreewindow);
			ImGui::MenuItem("Bin Editor", NULL, &opt_showbingenwindow);
			ImGui::MenuItem("USB Send", NULL, &opt_showusbsendwindow);

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

static void ShowRootWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	if (opt_fullscreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("-", p_open, window_flags);

	if (opt_fullscreen) {
		ImGui::PopStyleVar();	// WindowBorderSize
		ImGui::PopStyleVar();	// WindowRounding
	}
	ImGui::PopStyleVar();	// WindowPadding

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

	ShowRootWindowMenu();

	ImGui::End();
	if (opt_showdemowindow) {
		ImGui::ShowDemoWindow(&opt_showdemowindow);
	}

	if (opt_showusbdevicetreewindow) {
		ShowUSBDeviceTreeWindow(&opt_showusbdevicetreewindow);
	}

	if (opt_showusbsendwindow) {
		ShowUSBSendWindow(&opt_showusbsendwindow);
	}

	if (opt_showbingenwindow) {
		ShowBinGenWindow(&opt_showbingenwindow);
	}

	if (opt_showlogwindow) {
		ShowLog(&opt_showlogwindow);
	}

	if (opt_showbinviewerwindow) {
		ShowBinViewer(&opt_showbinviewerwindow);
	}
}

static int main_gui()
{
	ShowRootWindow(&show_root_window);

	static long frame_cnt = 0;
	if (frame_cnt % 100 == 0)
		Saveini(ini_path);
	frame_cnt++;

    return 0;
}


#include "gui_glue_gl3.cpp"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int nShowCmd)
{
    return main_(__argc, __argv);
}

int main()
{
	return main_(__argc, __argv);
}