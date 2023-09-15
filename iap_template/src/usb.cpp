#include "usb.h"

#include <assert.h>

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

	if (iv == std::numeric_limits<size_t>::max() ||
		ip == std::numeric_limits<size_t>::max())
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


HIDFindResultEnum OpenHIDInterface(uint16_t vid, uint16_t pid, HIDDevice* hid)
{
	hid_device_info* hid_device_list = hid_enumerate(0, 0);
	uint8_t descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	int res = 0;
	for (hid_device_info* p = hid_device_list; p != NULL; p = p->next)
	{
		if (ValidateVPID(p->path, vid, pid))
		{
			hid_device* dev = hid_open_path(p->path);

			res = hid_get_report_descriptor(dev, descriptor, sizeof(descriptor));
			if (res < 0)
				continue;
			if (!ValidateReportID(descriptor, res, hid))
				continue;
			printf("reportId: %04X\n", hid->reportId);
			if (!ValidateUsage(descriptor, res, hid))
				continue;
			printf("usage: %04X\n", hid->usage);

			if (hid->usage != 0xFF00)
				continue;

			hid->phandle = dev;
			return HID_FIND_SUCCESS;
		}
	}

	hid_free_enumeration(hid_device_list);
	return HID_OPEN_FAILED;
}

HIDFindResultEnum OpenHIDInterface(uint16_t vid, uint16_t pid, uint8_t reportId, HIDDevice* hid)
{
	hid_device_info* hid_device_list = hid_enumerate(0, 0);
	hid_device_info* p = hid_device_list;

	uint8_t descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	int res = 0;

	while (p != NULL)
	{
		if (ValidateVPID(p->path, vid, pid))
		{
			hid_device* dev = hid_open_path(p->path);

			res = hid_get_report_descriptor(dev, descriptor, sizeof(descriptor));
			if (res < 0)
				continue;
			if (!ValidateReportID(descriptor, res, hid))
				continue;
			printf("reportId: %04X\n", hid->reportId);
			if (!ValidateUsage(descriptor, res, hid))
				continue;
			printf("usage: %04X\n", hid->usage);

			if (hid->usage != 0xFF00)
				continue;
			if (reportId != hid->reportId)
				continue;

			hid->phandle = dev;
			return HID_FIND_SUCCESS;
		}
		p = p->next;
	}

	hid_free_enumeration(hid_device_list);

	return HID_OPEN_FAILED;
}

void CloseHIDInterface(HIDDevice hid)
{
	hid_close(hid.phandle);
}


