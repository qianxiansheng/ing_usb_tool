#include "usb.h"

#include <assert.h>
#include <tuple>

#include "util/utils.h"


bool HIDInit()
{
	return hid_init();
}

void HIDExit()
{
	hid_exit();
}

bool ValidateVPID(char* path, uint16_t vid, uint16_t pid)
{
	std::string p(path);
	size_t iv = p.find("VID_");
	size_t ip = p.find("PID_");

	if (iv == (std::numeric_limits<size_t>::max)() ||
		ip == (std::numeric_limits<size_t>::max)())
		return false;

	uint16_t tvid = utils::htoi_16(path + iv + 4);
	uint16_t tpid = utils::htoi_16(path + ip + 4);

	if (tvid == vid && tpid == pid)
		return true;
	else
		return false;
}

bool ValidateUsage(uint8_t* desc, uint32_t size, HIDDevice* hid)
{
	uint16_t i = 0;
	while (i < size)
	{
		if ((desc[i] & 0x0F) == 0x06)
		{
			if (desc[i] == 0x06)
			{
				hid->usage = desc[i + 1] | (desc[i + 2] << 8);
				return true;
			}
			i += 3;
		}
		else
		{
			i += 2;
		}
	}
	return false;
}

bool ValidateReportID(uint8_t* desc, uint32_t size, HIDDevice* hid)
{
	uint16_t i = 0;
	while (i < size)
	{
		if ((desc[i] & 0x0F) == 0x06)
		{
			i += 3;
		}
		else
		{
			if (desc[i] == 0x85)
			{
				hid->reportId = desc[i + 1];
				return true;
			}
			i += 2;
		}
	}
	return false;
}

static bool OpenValidHIDInterface(hid_device_info* p, uint16_t vid, uint16_t pid, uint8_t rid, HIDDevice* hid)
{
	if (!ValidateVPID(p->path, vid, pid))
		return false;

	hid_device* dev = hid_open_path(p->path);
	if (!dev)
		return false;

	uint8_t descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	int res = hid_get_report_descriptor(dev, descriptor, sizeof(descriptor));
	if (res < 0)
		return false;
	if (!ValidateReportID(descriptor, res, hid))
		return false;
	printf("reportId: %04X\n", hid->reportId);
	if (!ValidateUsage(descriptor, res, hid))
		return false;
	printf("usage: %04X\n", hid->usage);

	if (hid->usage != 0xFF00)
		return false;
	if (hid->reportId != rid)
		return false;

	hid->phandle = dev;
	return true;
}

HIDFindResultEnum OpenHIDInterface(std::vector<std::tuple<uint16_t, uint16_t, uint8_t>>& vpridList, HIDDevice* hid)
{
	hid_device_info* hid_device_list = hid_enumerate(0, 0);
	hid_device_info* p = hid_device_list;

	for (hid_device_info* p = hid_device_list; p != NULL; p = p->next) {
		for (const auto& vprid : vpridList) {
			auto& [vid, pid, rid] = vprid;
			if (OpenValidHIDInterface(p, vid, pid, rid, hid)) {
				return HID_FIND_SUCCESS;
			}
		}
	}
	hid_free_enumeration(hid_device_list);
	return HID_OPEN_FAILED;
}

void CloseHIDInterface(HIDDevice hid)
{
	hid_close(hid.phandle);
}

bool GetUSBDeviceInfo(std::vector<std::tuple<uint16_t, uint16_t, uint8_t>>& vpridList, uint16_t* VID, uint16_t* PID, uint16_t* bcdDevice)
{
	bool finded = false;
	size_t cnt;

	libusb_context* libusb_ctx = NULL;
	libusb_device** libusb_device_list = NULL;

	struct libusb_device_descriptor desc {};

	// init libusb
	if (libusb_init(&libusb_ctx) < 0)
		throw std::exception("failed to init libusb.");

	// enumerate all device
	cnt = libusb_get_device_list(libusb_ctx, &libusb_device_list);

	for (int i = 0; i < cnt; ++i) {
		for (const auto& vprid : vpridList) {
			auto& [vid, pid, rid] = vprid;
			if (libusb_get_device_descriptor(libusb_device_list[i], &desc)) {
				throw std::exception("failed to get device descriptor");
			}

			if (vid == desc.idVendor && pid == desc.idProduct) {
				*VID = desc.idVendor;
				*PID = desc.idProduct;
				*bcdDevice = desc.bcdDevice;
				finded = true;
				break;
			}
		}
		if (finded) {
			break;
		}
	}

	// free
	libusb_free_device_list(libusb_device_list, 1);
	libusb_exit(libusb_ctx);

	return finded;
}
