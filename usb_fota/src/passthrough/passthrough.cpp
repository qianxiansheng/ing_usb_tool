#include "passthrough.h"

#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "libusb-1.0/libusb.h"

#include "util/utils.h"

#include "imgui/extensions/ImConsole.h"

extern ImLogger* logger;

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

char vidText[5] = "FFFF";
char pidText[5] = "FA22";
char interfaceText[5] = "0";
char epOutText[3] = "02";
char epInText[3] = "81";
char writeSizeText[32] = "64";
char readSizeText[32] = "64";
char sendText[DATA_INPUT_BUFF_MAX_SIZE] = "0x3F30313233";
char recvText[DATA_INPUT_BUFF_MAX_SIZE] = "";

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
	nRet = libusb_bulk_transfer(pHandle, ep, (unsigned char*)data, (int)size, &nActualBytes, 1000);
	if (nRet < 0)
	{
		libusb_release_interface(pHandle, interfaceIndex);
		libusb_close(pHandle);
		libusb_exit(temp_ctx);
		throw std::exception((std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") write failed:[" << libusb_strerror(nRet) << "] \n").str().c_str());
	}

	logger->AddLog("[info] %s\n", (std::stringstream() << "libusb_bulk_transfer(0x" << std::hex << (int)ep << std::dec << ") write size:[" << nActualBytes << "]").str().c_str());

	unsigned char hexbuf[DATA_INPUT_BUFF_MAX_SIZE * 2] = { 0 };
	utils::Hex2String(data, hexbuf, (int)size);
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
	nRet = libusb_bulk_transfer(pHandle, ep, (unsigned char*)data, (int)size, &nActualBytes, 10000);
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

void ShowUSBSendWindow(bool* p_open)
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
	ImGuiDCXAxisAlign(100); ImGui::InputTextEx("##VID", NULL, vidText, sizeof(vidText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::Text("PID");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextEx("##PID", NULL, pidText, sizeof(pidText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::Text("INTERFACE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextEx("##INTERFACE", NULL, interfaceText, sizeof(interfaceText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::Text("EP OUT");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextEx("##EP OUT", NULL, epOutText, sizeof(epOutText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::SameLine();
	ImGuiDCXAxisAlign(220); ImGui::Text("WRITE SIZE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(320); ImGui::InputTextEx("##WRITE SIZE", NULL, writeSizeText, sizeof(writeSizeText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::SameLine();
	btnSend = ImGui::Button("Write");
	ImGui::SameLine();
	static bool autoRecv = true;
	ImGui::Checkbox("Auto Recv", &autoRecv);

	ImGui::Text("EP IN");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextEx("##EP IN", NULL, epInText, sizeof(epInText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::SameLine();
	ImGuiDCXAxisAlign(220); ImGui::Text("READ SIZE");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(320); ImGui::InputTextEx("##READ SIZE", NULL, readSizeText, sizeof(readSizeText), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsDecimal);
	ImGui::SameLine();
	btnRecv = ImGui::Button("Receive");

	static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
	ImGui::Text("DATA Send");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextMultiline("##DATA SEND", sendText, sizeof(sendText), ImVec2(500.0f, ImGui::GetTextLineHeight() * 5), flags);

	ImGui::Text("DATA Recv");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(100); ImGui::InputTextMultiline("##DATA RECV", recvText, sizeof(recvText), ImVec2(500.0f, ImGui::GetTextLineHeight() * 5), flags);


	utils::ValidateU16Text(vidText, vid);
	utils::ValidateU16Text(pidText, pid);
	utils::ValidateIntText(interfaceText, interface);
	utils::ValidateU8Text(epOutText, epOut);
	utils::ValidateU8Text(epInText, epIn);
	utils::ValidateIntText(writeSizeText, write_size);
	utils::ValidateIntText(readSizeText, read_size);
	utils::ValidateDataText(sendText, sendData, data_size);
	
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
	if (btnRecv || (btnSend && autoRecv))
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
