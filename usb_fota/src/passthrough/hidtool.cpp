#include "hidtool.h"

#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "libusb-1.0/libusb.h"

#include "util/utils.h"
#include "util/thread_pool.h"
#include "usb/usb_utils.h"

#include "imgui/extensions/ImConsole.h"

extern ImLogger* logger;
extern ThreadPool* pool;

extern std::unordered_map<uint32_t, usb::device> deviceMap;

struct hid_descriptor_t
{
	uint8_t    size;
	uint8_t    type;
	uint16_t   bcd;
	uint8_t    countryCode;
	uint8_t    nbDescriptor;
	uint8_t    classType0;
	uint16_t   classlength0;
};

struct hid_reprot_descriptor_t
{
	uint8_t a;
};

struct hid_interface_t
{
	usb::interface_t interface_;

	std::string name;

	uint16_t dev_vid;
	uint16_t dev_pid;

	hid_descriptor_t hid_descriptor;
	std::vector<uint8_t> hid_descriptor_buf;

	hid_reprot_descriptor_t report_descriptor;
	std::vector<uint8_t> report_descriptor_buf;
};

std::unordered_map<std::string, hid_interface_t> hidInterfaceMap;


#define USB_RESULT_BEGIN(b) if (b) {
#define USB_RESULT_END() logger->AddLog("[hid tool] [error] %s\n", libusb_strerror(nRet)); \
return; }

hid_descriptor_t ParseHIDDescriptor(uint8_t* buf)
{
	hid_descriptor_t d;
	d.size			= buf[0];
	d.type			= buf[1];
	d.bcd			= buf[2] << 8 | buf[3];
	d.countryCode	= buf[4];
	d.nbDescriptor	= buf[5];
	d.classType0	= buf[6];
	d.classlength0	= buf[7] << 8 | buf[8];
	return d;
}

void GetHIDReportDesc(hid_interface_t& hidi)
{
	int nRet = 0;
	libusb_context* temp_ctx = NULL;
	nRet = libusb_init(&temp_ctx);

	USB_RESULT_BEGIN(nRet < 0);
	USB_RESULT_END();

	libusb_device_handle* pHandle =
		libusb_open_device_with_vid_pid(temp_ctx, hidi.dev_vid, hidi.dev_pid);

	USB_RESULT_BEGIN(pHandle == NULL);
	libusb_exit(temp_ctx);
	USB_RESULT_END();

	nRet = libusb_claim_interface(pHandle, hidi.interface_.bInterfaceNumber);

	USB_RESULT_BEGIN(nRet < 0);
	libusb_close(pHandle);
	libusb_exit(temp_ctx);
	USB_RESULT_END();

	hidi.hid_descriptor_buf.resize(9);

	nRet = libusb_get_descriptor(pHandle,
		USB_REQUEST_HID_CLASS_DESCRIPTOR_HID, hidi.interface_.bInterfaceNumber,
		hidi.hid_descriptor_buf.data(), hidi.hid_descriptor_buf.size());

	USB_RESULT_BEGIN(nRet < 0);
	libusb_release_interface(pHandle, hidi.interface_.bInterfaceNumber);
	libusb_close(pHandle);
	libusb_exit(temp_ctx);
	USB_RESULT_END();

	hidi.hid_descriptor = ParseHIDDescriptor(hidi.hid_descriptor_buf.data());

	hidi.report_descriptor_buf.resize(hidi.hid_descriptor.classlength0);

	nRet = libusb_get_descriptor(pHandle,
		USB_REQUEST_HID_CLASS_DESCRIPTOR_REPORT, hidi.interface_.bInterfaceNumber,
		hidi.report_descriptor_buf.data(), hidi.report_descriptor_buf.size());

	USB_RESULT_BEGIN(nRet < 0);
	libusb_release_interface(pHandle, hidi.interface_.bInterfaceNumber);
	libusb_close(pHandle);
	libusb_exit(temp_ctx);
	USB_RESULT_END();

	logger->AddLog("[hid tool] ok\n");
}

void GetHIDInterface()
{
	char buf[128] = { 0 };

	for (const auto& devPair : deviceMap)
	{
		const auto& dev = devPair.second;

		for (const auto& configuration : dev.configurationList)
		{
			for (const auto& interface_ : configuration.interfaceList)
			{
				if (interface_.bInterfaceClass == 0x03)
				{
					hid_interface_t hid_interface_;
					sprintf(buf, "hid#vid_%04x&pid_%04x", dev.vid, dev.pid);
					hid_interface_.name = buf;
					hid_interface_.dev_vid = dev.vid;
					hid_interface_.dev_pid = dev.pid;
					hid_interface_.interface_ = interface_;

					GetHIDReportDesc(hid_interface_);

					hidInterfaceMap[hid_interface_.name] = hid_interface_;
				}
			}
		}
	}
	std::cout << hidInterfaceMap.size() << std::endl;
}

void RefreshHIDInterface()
{
	usb::enumerate_all_device();

	GetHIDInterface();
}

void InitHIDTool()
{
	GetHIDInterface();
}


void ShowHIDToolWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("HID tool", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
	
	const char* items[] = { "AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLLLLL", "MMMM", "OOOOOOO" };
	struct Funcs { static bool ItemGetter(void* data, int n, const char** out_str) { *out_str = ((const char**)data)[n]; return true; } };
	static int item_current_4 = 0;
	ImGui::Combo("combo 4 (function)", &item_current_4, &Funcs::ItemGetter, items, IM_ARRAYSIZE(items));

	if (ImGui::Button("refresh##REFRESH_HID_INTERFACES"))
	{
		pool->enqueue(RefreshHIDInterface);
	}

	ImGui::End();
}