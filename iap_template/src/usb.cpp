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
	hid->phandle = hid_open(vid, pid, NULL);
	if (hid->phandle == NULL)
		return HID_OPEN_FAILED;

	uint8_t descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	int res = 0;

	res = hid_get_report_descriptor(hid->phandle, descriptor, sizeof(descriptor));
	if (res < 0)
		return HID_FAILEDTO_GET_REPORT_DESC;

	if (!ValidateReportID(descriptor, res, hid))
		return HID_REPORT_DESC_NOT_MATCH;

	return HID_FIND_SUCCESS;
}
HIDFindResultEnum OpenHIDInterface(uint16_t vid, uint16_t pid, uint8_t reportId, HIDDevice* hid)
{
	hid->phandle = hid_open(vid, pid, NULL);
	if (hid->phandle == NULL)
		return HID_OPEN_FAILED;

	uint8_t descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	int res = 0;

	res = hid_get_report_descriptor(hid->phandle, descriptor, sizeof(descriptor));
	if (res < 0)
		return HID_FAILEDTO_GET_REPORT_DESC;

	if (!ValidateReportID(descriptor, res, hid))
		return HID_REPORT_DESC_NOT_MATCH;

	if (reportId != hid->reportId)
		return HID_REPORT_DESC_NOT_MATCH;

	return HID_FIND_SUCCESS;
}

void CloseHIDInterface(HIDDevice hid)
{
	hid_close(hid.phandle);
}


