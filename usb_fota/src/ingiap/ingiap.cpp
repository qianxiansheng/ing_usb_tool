#include "ingiap.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "libusb-1.0/libusb.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "imgui/extensions/ImConsole.h"
#include "imgui/extensions/ImFileDialog.h"

#include "util/utils.h"
#include "util/thread_pool.h"
#include "usb/usb_utils.h"

extern ImLogger* logger;

#define IAP_PANEL_TITLE "Progress"

static uint32_t progress_limit = 1;
static uint32_t progress_pos = 0;

static std::filesystem::path iap_bin_path;
static std::vector<uint8_t> iap_bin_data;
static char iap_bin_path_gbk[128];
static char iap_bin_path_utf8[128];

static bool iap_bin_selected = false;
static uint32_t iap_bin_size;
static uint32_t iap_bin_origin_size;
static uint16_t check_value;
static uint16_t block_size;
static char hard_version[7] = "";
static char soft_version[7] = "";
static uint16_t iap_block_size = 0;
static uint16_t iap_block_num = 0;
static char block_size_buf[32] = "";
static char delay_buf[32] = "1000";
static uint16_t iap_delay = 0;

static char vid_buf[5] = "FFFF";
static char pid_buf[5] = "FA22";
static uint16_t vid = 0;
static uint16_t pid = 0;

static libusb_context* libusb_iap_ctx = NULL;
static libusb_device_handle* pHandle = NULL;
static uint16_t active_vid = 0;
static uint16_t active_pid = 0;
static bool iapInterfaceFinded = false;
static int iapInterfaceIndex = 0;
static int ep_in = 1;
static int ep_out = 2;

enum iap_status_e
{
	IAP_STATUS_SEND_START = 0,
	IAP_STATUS_SEND_WRITE_FLASH,
	IAP_STATUS_SEND_REBOOT,
	IAP_STATUS_SEND_SWITCH_APP,
	IAP_STATUS_COMPLETE,
	IAP_STATUS_TERMINATE,
};
static iap_status_e iap_status = IAP_STATUS_SEND_START;

static uint8_t hid_report_buf[IAP_USB_HID_REPORT_SIZE] = { IAP_USB_HID_REPORT_ID };
static uint8_t businessFlashWriteBuf[IAP_PRO_BUSINESS_FLASH_WRITE_BUF_MAX_SIZE] = { 0 };
static uint8_t businessAckBuf[IAP_PRO_BUSINESS_ACK_MAX_SIZE] = { 0 };

extern ThreadPool* pool;
extern std::filesystem::path export_dir;
extern std::unordered_map<uint32_t, usb::device> deviceMap;

static void ShowIAPPanel()
{
	if (ImGui::BeginPopupModal(IAP_PANEL_TITLE, NULL, ImGuiWindowFlags_None))
	{
		if (iap_status == IAP_STATUS_SEND_START)		ImGui::Text("handshaking...");
		if (iap_status == IAP_STATUS_SEND_WRITE_FLASH)	ImGui::Text("write flash...");
		if (iap_status == IAP_STATUS_SEND_REBOOT)		ImGui::Text("rebooting...");
		if (iap_status == IAP_STATUS_SEND_SWITCH_APP)	ImGui::Text("switching...");
		if (iap_status == IAP_STATUS_COMPLETE)			ImGui::Text("completed.");
		if (iap_status == IAP_STATUS_TERMINATE)			ImGui::Text("terminated.");

		const float progress_global_size = 500;
		float progress_global = (float)progress_pos / progress_limit;
		float progress_global_saturated = std::clamp(progress_global, 0.0f, 1.0f);
		char buf_global[32];
		sprintf(buf_global, "%d/%d", (int)(progress_global_saturated * progress_limit), progress_limit);
		ImGui::ProgressBar(progress_global, ImVec2(progress_global_size, 0.f), buf_global);


		if (iap_status == IAP_STATUS_COMPLETE || iap_status == IAP_STATUS_TERMINATE)
		{
			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

static bool isIAPInterface(const usb::interface_t& interface_)
{
	if (interface_.bInterfaceClass != 0x03 && interface_.bNumEndpoints < 2)
		return false;

	bool has_out_ep = false;
	bool has_in_ep = false;
	uint8_t ep_out_ = 0;
	uint8_t ep_in_ = 0;

	for (const auto& endpoint : interface_.endpointList)
	{
		if (endpoint.bEndpointAddress & 0x80)
		{
			ep_in_ = endpoint.bEndpointAddress;
			has_in_ep = true;
		}
		else
		{
			ep_out_ = endpoint.bEndpointAddress;
			has_out_ep = true;
		}
	}
	if (has_out_ep && has_in_ep)
	{
		iapInterfaceIndex = interface_.bInterfaceNumber;
		ep_in = ep_in_;
		ep_out = ep_out_;
		iapInterfaceFinded = true;
		return true;
	}
	else
		return false;
}

static void OpenIAPDevice()
{
	char sbuf[64] = { 0 };
	int nRet = 0;

	if (libusb_iap_ctx == NULL)
		libusb_init(&libusb_iap_ctx);

	if (pHandle != NULL && (active_vid != vid || active_pid != pid))
	{
		iapInterfaceFinded = false;
		libusb_close(pHandle);
		pHandle = NULL;
	}

	if (pHandle == NULL)
		pHandle = libusb_open_device_with_vid_pid(libusb_iap_ctx, vid, pid);

	if (pHandle == NULL)
	{
		sprintf(sbuf, "libusb_open_device failed PID:%04x, VID:%04x", vid, pid);
		throw std::exception(sbuf);
	}
	logger->AddLog("[iap] libusb_open_device_with_vid_pid ok PID:%04x, VID:%04x\n", vid, pid);
	active_vid = vid;
	active_pid = pid;

	deviceMap = usb::enumerate_all_device();

	// search InterfaceIAP and EndpointIN and EndpointOUT
	uint32_t key = vid << 16 | pid;
	for (const auto& configuration : deviceMap[key].configurationList)
	{
		for (const auto& interface_ : configuration.interfaceList)
		{
			isIAPInterface(interface_);

			if (iapInterfaceFinded) break;
		}
		if (iapInterfaceFinded) break;
	}

	nRet = libusb_claim_interface(pHandle, iapInterfaceIndex);
	if (nRet < 0)
	{
		sprintf(sbuf, "libusb_claim_interface failed index:%d", iapInterfaceIndex);
		throw std::exception(sbuf);
	}
	logger->AddLog("[iap] libusb_claim_interface ok index:%d\n", iapInterfaceIndex);
}


int hid_set_report(uint8_t* data, uint32_t size)
{
	assert(size <= IAP_USB_HID_REPORT_CONTENT_SIZE);
	assert(pHandle != NULL);
	if (hid_report_buf + 1 != data)
		memcpy(hid_report_buf + 1, data, size);

	int nRet = 0;
	int actualLength = 0;
	nRet = libusb_interrupt_transfer(pHandle, ep_out & ~0x80, hid_report_buf, IAP_USB_HID_REPORT_SIZE, &actualLength, 0);
	if (nRet < 0) return nRet;
	return actualLength;
}
int hid_get_report(uint8_t* data, uint32_t size)
{
	assert(size <= IAP_USB_HID_REPORT_CONTENT_SIZE);
	assert(pHandle != NULL);

	int nRet = 0;
	int actualLength = 0;
	nRet = libusb_interrupt_transfer(pHandle, ep_in | 0x80, hid_report_buf, IAP_USB_HID_REPORT_SIZE, &actualLength, 0);
	if (data != hid_report_buf + 1)
		memcpy(data, hid_report_buf + 1, size);
	
	if (nRet < 0) return nRet;
	return actualLength;
}

enum iap_cmd_e
{
	IAP_CMD_START = 0xA0,
	IAP_CMD_FLASH_WRITE = 0xA1,
	IAP_CMD_FLASH_READ = 0xA2,
	IAP_CMD_REBOOT = 0xA3,
	IAP_CMD_SWITCH_APP = 0xA4,

	IAP_CMD_ACK = 0xB0,
};

enum iap_transfer_ctrl_flag_bit
{
	IAP_TRANSFER_DIRECT_UP = 0x80,
	IAP_TRANSFER_DIRECT_DOWN = 0x00,
	IAP_TRANSFER_PACK_TYPE_DATA = 0x00,
	IAP_TRANSFER_PACK_TYPE_ACK = 0x40,
};

enum iap_business_ack_code_e
{
	ACK_CODE_SUCCESS					= 0x00,
	ACK_CODE_UNKNOWN_CMD				= 0xE0,
	ACK_CODE_LENGTH_ERORR				= 0xE1,
	ACK_CODE_CRC_ERROR					= 0xE2,
	ACK_CODE_BLOCK_NUM_ERROR			= 0xE3,
	ACK_CODE_BLOCK_SIZE_ERROR			= 0xE4,
	ACK_CODE_WRITE_OFFSET_ERROR			= 0xE5,
	ACK_CODE_READ_OFFSET_ERROR			= 0xE6,
	ACK_CODE_ARGUMENT_ERROR				= 0xE7,
	ACK_CODE_FLASH_OPERATION_FAILED		= 0xE8,
	ACK_CODE_STATUS_ERROR				= 0xE9,

	ACK_CODE_HEADER_IDENTIFY_ERROR		= 0xF0,
	ACK_CODE_HEADER_CHIP_ID_ERROR		= 0xF1,
	ACK_CODE_HEADER_ITEM_INFO_ERROR		= 0xF2,
	ACK_CODE_HEADER_HW_VERSION_ERROR	= 0xF3,
	ACK_CODE_HEADER_SW_VERSION_ERROR	= 0xF4,
	ACK_CODE_HEADER_CHECK_INFO_ERROR	= 0xF5,
	ACK_CODE_HEADER_BLOCK_INFO_ERROR	= 0xF6,
	ACK_CODE_HEADER_UPGRADE_TYPE_ERROR	= 0xF7,
	ACK_CODE_HEADER_ENCRYPT_ERROR		= 0xF8,
};

class transfer_pack_exception : public std::exception {};
class business_pack_exception : public std::exception {};

void iap_transfer(uint8_t* data, uint32_t size, uint16_t packIdxReverse, bool isFirst)
{
	assert(size <= IAP_PRO_TRANSFER_PAYLOAD_SIZE);
	uint8_t* buf = hid_report_buf + 1;
	buf[0] = IAP_PRO_TRANSFER_HEADER;
	buf[1] = IAP_TRANSFER_DIRECT_DOWN | IAP_TRANSFER_PACK_TYPE_DATA;
	uint16_t pack_ctrl = (isFirst << 15) | (packIdxReverse & 0x7FFF);
	buf[2] = pack_ctrl & 0xFF;
	buf[3] = pack_ctrl >> 8;
	buf[4] = size;
	buf[5] = size >> 8;
	if (buf + 6 != data)
		memcpy(buf + 6, data, size);
	buf[6 + size] = utils::bcc(buf + 1, size + 5);


	char sbuf[64] = { 0 };
	int nRet = hid_set_report(buf, size);
	if (nRet < 0)
	{
		sprintf(sbuf, "hid_set_report failed %s", libusb_strerror(nRet));
		throw std::exception(sbuf);
	}
}

void iap_business(uint8_t* data, uint32_t size)
{
	assert(size >= 0);
	uint16_t packSize     = IAP_PRO_TRANSFER_PAYLOAD_SIZE;
	uint32_t packNum      = (size - 1) / packSize + 1;
	uint16_t packLastSize = (size - 1) % packSize + 1;

	uint16_t currentPackSize = 0;
	uint32_t packIdx = 0;
	uint32_t packIdxReverse = 0;

	for (packIdx = 0; packIdx < packNum; ++packIdx)
	{
		currentPackSize = (packIdx == packNum - 1) ? packLastSize : packSize;
		packIdxReverse = packNum - packIdx - 1;

		iap_transfer(data + (packIdx * packSize), currentPackSize, packIdxReverse, (packIdx == 0));
	}

	char hexbuf[33] = { 0 };
	utils::Hex2String(data, (unsigned char*)hexbuf, 16);
	logger->AddLog("[Pack send]: %s ... (size:%d)\n", hexbuf, size);
}

uint16_t iap_transfer_read(uint8_t* data, bool* isFirst)
{
	uint8_t* buf = hid_report_buf + 1;
	uint16_t len = 0;
	int actualLen = hid_get_report(buf, IAP_USB_HID_REPORT_CONTENT_SIZE);

	if (buf[0] != IAP_PRO_TRANSFER_HEADER)
		throw transfer_pack_exception();

	len = buf[4] | (buf[5] << 8);
	//if (len != actualLen - IAP_PRO_TRANSFER_ATT_SIZE)
	//	throw transfer_pack_exception();

	if (buf[len + 6] != utils::bcc(buf + 1, len + 5))
		throw transfer_pack_exception();

	*isFirst = buf[3] & 0x80;

	if (data != buf)
		memcpy(data, buf + 6, len);

	return len;
}

uint16_t iap_business_read(uint8_t* data, size_t size)
{
	bool isFirst = false;
	bool isLastPack = false;
	uint16_t transferPackLen = 0;
	uint16_t businessPackLen = 0;
	uint16_t i = 0;
	do
	{
		transferPackLen = iap_transfer_read(data + i, &isFirst);
		if (isFirst)
		{
			if (transferPackLen < IAP_PRO_BUSINESS_ACK_ATT_SIZE)
				throw business_pack_exception();

			businessPackLen = data[3] | (data[4] << 8);

			if (businessPackLen > IAP_PRO_BUSINESS_ACK_PAYLOAD_MAX_SIZE)
				throw business_pack_exception();
		}

		i += transferPackLen;

		if (i == businessPackLen + IAP_PRO_BUSINESS_ACK_ATT_SIZE)
			isLastPack = true;

		utils::my_sleep(1);
	} while (!isLastPack);


	if (data[0] != IAP_CMD_ACK)
		throw business_pack_exception();

	uint16_t crc_calc = utils::crc16_modbus(data, 5 + businessPackLen);
	uint16_t crc = data[businessPackLen + 5] | (data[businessPackLen + 6] << 8);
	if (crc != crc_calc)
		throw business_pack_exception();

	return businessPackLen + IAP_PRO_BUSINESS_ACK_ATT_SIZE;
}

int iap_get_flash_read_ack()
{
	try
	{
		return iap_business_read(businessAckBuf, sizeof(businessAckBuf));
	}
	catch (::transfer_pack_exception)
	{
		logger->AddLog("[ACK] transfer pack exception\n");
	}
	catch (::business_pack_exception)
	{
		logger->AddLog("[ACK] business pack exception\n");
	}
	return -1;
}

int iap_get_ack()
{
	try
	{
		uint16_t ack_len = iap_business_read(businessAckBuf, sizeof(businessAckBuf));

		return businessAckBuf[1];
	}
	catch (::transfer_pack_exception)
	{
		logger->AddLog("[ACK] transfer pack exception\n");
	}
	catch (::business_pack_exception)
	{
		logger->AddLog("[ACK] business pack exception\n");
	}
	return -1;
}

std::string iap_ack_str(iap_business_ack_code_e c)
{
	switch (c)
	{
	case ACK_CODE_SUCCESS:
		return "ACK_CODE_SUCCESS";
	case ACK_CODE_UNKNOWN_CMD:
		return "ACK_CODE_UNKNOWN_CMD";
	case ACK_CODE_LENGTH_ERORR:
		return "ACK_CODE_LENGTH_ERORR";
	case ACK_CODE_CRC_ERROR:
		return "ACK_CODE_CRC_ERROR";
	case ACK_CODE_BLOCK_NUM_ERROR:
		return "ACK_CODE_BLOCK_NUM_ERROR";
	case ACK_CODE_BLOCK_SIZE_ERROR:
		return "ACK_CODE_BLOCK_SIZE_ERROR";
	case ACK_CODE_WRITE_OFFSET_ERROR:
		return "ACK_CODE_WRITE_OFFSET_ERROR";
	case ACK_CODE_READ_OFFSET_ERROR:
		return "ACK_CODE_READ_OFFSET_ERROR";
	case ACK_CODE_ARGUMENT_ERROR:
		return "ACK_CODE_ARGUMENT_ERROR";
	case ACK_CODE_FLASH_OPERATION_FAILED:
		return "ACK_CODE_FLASH_OPERATION_FAILED";
	case ACK_CODE_STATUS_ERROR:
		return "ACK_CODE_STATUS_ERROR";
	case ACK_CODE_HEADER_IDENTIFY_ERROR:
		return "ACK_CODE_HEADER_IDENTIFY_ERROR";
	case ACK_CODE_HEADER_CHIP_ID_ERROR:
		return "ACK_CODE_HEADER_CHIP_ID_ERROR";
	case ACK_CODE_HEADER_ITEM_INFO_ERROR:
		return "ACK_CODE_HEADER_ITEM_INFO_ERROR";
	case ACK_CODE_HEADER_HW_VERSION_ERROR:
		return "ACK_CODE_HEADER_HW_VERSION_ERROR";
	case ACK_CODE_HEADER_SW_VERSION_ERROR:
		return "ACK_CODE_HEADER_SW_VERSION_ERROR";
	case ACK_CODE_HEADER_CHECK_INFO_ERROR:
		return "ACK_CODE_HEADER_CHECK_INFO_ERROR";
	case ACK_CODE_HEADER_UPGRADE_TYPE_ERROR:
		return "ACK_CODE_HEADER_UPGRADE_TYPE_ERROR";
	case ACK_CODE_HEADER_ENCRYPT_ERROR:
		return "ACK_CODE_HEADER_ENCRYPT_ERROR";
	default:
		return "ACK_CODE_UNKNOWN_ERROR";
	}
}

void iap_send_business_ack(uint8_t* data, uint32_t size)
{
	int cnt = 0;
	int code = 0;
	do
	{
		iap_business(data, size);

		code = iap_get_ack();

		logger->AddLog("[ack]: %s\n", iap_ack_str((iap_business_ack_code_e)code).c_str());

		utils::my_sleep(1);

		if (++cnt >= 3)
		{
			iap_status = IAP_STATUS_TERMINATE;
			break;
		}
	} while (code != ACK_CODE_SUCCESS);
}

void iap_start_upgrade()
{
	iap_status = IAP_STATUS_SEND_START;

	uint8_t buf[1 + 2 + IAP_HEADER_SIZE + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_START;									// CMD
	buf[i++] = IAP_HEADER_SIZE;									// LENGTH
	buf[i++] = IAP_HEADER_SIZE >> 8;
	memcpy(buf + i, iap_bin_data.data(), IAP_HEADER_SIZE);		// DATA
	i += IAP_HEADER_SIZE;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = crc & 0xFF;										// CRC
	buf[i++] = crc >> 8;

	iap_send_business_ack(buf, i);
}

void iap_flash_write()
{
	iap_status = IAP_STATUS_SEND_WRITE_FLASH;

	uint16_t i = 0;
	uint8_t* buf = businessFlashWriteBuf;
	uint8_t* binData = iap_bin_data.data() + 128;
	uint32_t binSize = iap_bin_origin_size;

	progress_limit = binSize;
	progress_pos = 0;

	uint16_t wBlockSize = block_size;
	uint32_t wBlockNum  = (binSize - 1) / wBlockSize + 1;
	uint16_t wBlockLast = (binSize - 1) % wBlockSize + 1;

	buf[i] = IAP_CMD_FLASH_WRITE;

	for (uint32_t wBlockIdx = 0; wBlockIdx < wBlockNum; ++wBlockIdx)
	{
		i = 1;
		uint16_t currentSize = (wBlockIdx == wBlockNum - 1) ? wBlockLast : wBlockSize;
		uint16_t length = currentSize + 6;
		uint32_t offset = wBlockIdx * wBlockSize;
		buf[i++] = length & 0xFF;
		buf[i++] = length >> 8;
		if (wBlockIdx == wBlockNum - 1)
		{
			buf[i++] = 0xFF;
			buf[i++] = 0xFF;
		}
		else
		{
			buf[i++] = wBlockIdx;
			buf[i++] = wBlockIdx >> 8;
		}
		buf[i++] = offset;
		buf[i++] = offset >> 8;
		buf[i++] = offset >> 16;
		buf[i++] = offset >> 24;
		memcpy(buf + i, binData + (wBlockIdx * wBlockSize), currentSize);		// DATA
		i += currentSize;
		uint16_t crc = utils::crc16_modbus(buf, i);
		buf[i++] = crc & 0xFF;
		buf[i++] = crc >> 8;

		if (iap_status == IAP_STATUS_TERMINATE) return;
		iap_send_business_ack(buf, i);

		progress_pos += currentSize;
	}
}

void iap_reboot()
{
	iap_status = IAP_STATUS_SEND_REBOOT;

	uint8_t buf[1 + 2 + 2 + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_REBOOT;									// CMD
	buf[i++] = 2;												// LENGTH
	buf[i++] = 0;
	buf[i++] = iap_delay & 0xFF;
	buf[i++] = iap_delay >> 8;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = crc & 0xFF;												// CRC
	buf[i++] = crc >> 8;
	iap_send_business_ack(buf, i);
}

void iap_switch_app()
{
	iap_status = IAP_STATUS_SEND_SWITCH_APP;

	uint8_t buf[1 + 2 + 2 + 2] = { 0 };
	uint16_t i = 0;
	buf[i++] = IAP_CMD_SWITCH_APP;						// CMD
	buf[i++] = 2;										// LENGTH
	buf[i++] = 0;
	buf[i++] = iap_delay & 0xFF;
	buf[i++] = iap_delay >> 8;
	uint16_t crc = utils::crc16_modbus(buf, i);
	buf[i++] = crc & 0xFF;								// CRC
	buf[i++] = crc >> 8;
	iap_send_business_ack(buf, i);
}

void StartIAP()
{
	iap_start_upgrade();
	if (iap_status == IAP_STATUS_TERMINATE) return;
	iap_flash_write();
	if (iap_status == IAP_STATUS_TERMINATE) return;
	iap_switch_app();

	iap_status = IAP_STATUS_COMPLETE;
}

void ShowIAPWindow(bool* p_open)
{
	bool show_alert_invalid_bin = false;
	bool show_alert_device_open_error = false;
	bool show_alert_device_open_success = false;

	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("IAP", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	float wfp = style.FramePadding.x;
	float wfpm2 = wfp * 2;

	float wl = 800.0f;
	float wc1_1 = 400.0f;
	float wc1_2 = wl - wc1_1;
	float wc2_1 = 100.0f;
	float wc2_2 = wl - wc2_1;
	float ac1_2 = wfpm2 + wc1_1;
	float ac2_2 = wfpm2 + wc2_1;
	float hbtn = 50.0f;

	ImGui::Text("VID:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac1_2);
	ImGui::SetNextItemWidth(wc1_2);
	ImGui::InputText("##OPEN_USB_VID", vid_buf, sizeof(vid_buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	utils::ValidateU16Text(vid_buf, vid);


	ImGui::Text("PID:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac1_2);
	ImGui::SetNextItemWidth(wc1_2);
	ImGui::InputText("##OPEN_USB_PID", pid_buf, sizeof(pid_buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	utils::ValidateU16Text(pid_buf, pid);

	if (ImGui::Button("Open Device", ImVec2(wl, hbtn)))
	{
		try 
		{
			OpenIAPDevice();
		}
		catch (std::exception e)
		{
			logger->AddLog("[open] %s\n", e.what());
		}

		if (iapInterfaceFinded)
			show_alert_device_open_success = true;
		else
			show_alert_device_open_error = true;
	}

	ImGui::Text("File:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	ImGui::InputText("##IAP_FILE_NAME", iap_bin_path_utf8, sizeof(iap_bin_path_utf8), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);
	
	ImGui::Text("Size:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	static char buf3[32] = "";
	sprintf(buf3, "%d", iap_bin_size);
	ImGui::InputText("##IAP_FILE_SIZE", buf3, sizeof(buf3), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);

	ImGui::Text("Check:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	static char buf4[5] = "";
	sprintf(buf4, "%04X", check_value);
	ImGui::InputText("##IAP_FILE_CHECK", buf4, sizeof(buf4), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);

	ImGui::Text("Hard Version:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	ImGui::InputText("##IAP_FILE_HARD_VERSION", hard_version, sizeof(hard_version), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);

	ImGui::Text("Soft Version:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	ImGui::InputText("##IAP_FILE_SOFT_VERSION", soft_version, sizeof(soft_version), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);

	ImGui::Text("Block Size:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	ImGui::InputText("##BLOCK_SIZE", block_size_buf, sizeof(block_size_buf), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CharsUppercase);

	ImGui::Text("Delay Reboot:");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(ac2_2);
	ImGui::SetNextItemWidth(wc2_2);
	ImGui::InputText("##DELAY_REBOOT", delay_buf, sizeof(delay_buf), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsUppercase);
	iap_delay = std::atoi(delay_buf);

	if (ImGui::Button("Select File", ImVec2(wc1_1 - wfp, hbtn)))
	{
		ifd::FileDialog::Instance().Open("IAP_BIN_OPEN", "Choose bin", "bin {.bin},.*", 
			false, utils::gbk_to_utf8(export_dir.generic_string()));
	}
	if (ifd::FileDialog::Instance().IsDone("IAP_BIN_OPEN")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			if (res.size() > 0) {
				iap_bin_path = res[0];
				strcpy(iap_bin_path_utf8, res[0].u8string().c_str());
				strcpy(iap_bin_path_gbk, utils::utf8_to_gbk(iap_bin_path_utf8).c_str());

				if (std::filesystem::exists(iap_bin_path) &&
					std::filesystem::is_regular_file(iap_bin_path))
				{
					iap_bin_size = static_cast<uint32_t>(
						std::filesystem::file_size(iap_bin_path));
					iap_bin_origin_size = iap_bin_size - IAP_HEADER_SIZE;
					iap_bin_data.resize(iap_bin_size);
					utils::readFileData(iap_bin_path_gbk, iap_bin_data.data());

					uint8_t* header = iap_bin_data.data();

					if (memcmp((char*)header, BIN_CONFIG_DEFAULT_IDENTIFY, 
						strlen(BIN_CONFIG_DEFAULT_IDENTIFY)) == 0)
					{
						uint8_t check_type = header[60];
						uint8_t check_len = header[61];
						check_value = header[62] + (header[63] << 8);

						memcpy(hard_version, header + 48, 6); hard_version[6] = '\0';
						memcpy(soft_version, header + 54, 6); soft_version[6] = '\0';

						block_size = header[66] + (header[67] << 8);

						sprintf(block_size_buf, "%d", block_size);

						progress_limit = iap_bin_origin_size;

						iap_bin_selected = true;
					}
					else
					{
						iap_bin_selected = false;
						show_alert_invalid_bin = true;
					}
				}
			}
		}
		ifd::FileDialog::Instance().Close();
	}

	if (!iap_bin_selected || !iapInterfaceFinded) ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Upgrade", ImVec2(wc1_1 - wfp, hbtn)))
	{
		pool->enqueue(StartIAP);
		ImGui::OpenPopup(IAP_PANEL_TITLE);
	}
	if (!iap_bin_selected || !iapInterfaceFinded) ImGui::EndDisabled();

	ShowIAPPanel();

	utils::Alert(show_alert_invalid_bin, "Message##m1", "invalid IAP bin.");
	//utils::Alert(show_alert_device_open_success, "Message##m2", "invalid IAP bin.");
	utils::Alert(show_alert_device_open_error, "Message##m3", "device open failed.");
		
	ImGui::End();
}

void ReleaseIAPWindow()
{
	if (pHandle != NULL)
	{
		libusb_close(pHandle);
		pHandle = NULL;
	}

	if (libusb_iap_ctx != NULL)
	{
		libusb_exit(libusb_iap_ctx);
		libusb_iap_ctx = NULL;
	}
}