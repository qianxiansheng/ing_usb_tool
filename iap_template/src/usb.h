#pragma once

#include <iostream>

#include "hidapi.h"


enum HIDFindResultEnum
{
	HID_FIND_SUCCESS = 0,
	HID_OPEN_FAILED,
	HID_FAILEDTO_GET_REPORT_DESC,
	HID_REPORT_DESC_NOT_MATCH
};

struct HIDDevice
{
	hid_device* phandle;

	uint16_t vid = 0;
	uint16_t pid = 0;
	uint8_t reportId;
};

bool HIDInit();
HIDFindResultEnum OpenHIDInterface(uint16_t vid, uint16_t pid, HIDDevice* hid);
HIDFindResultEnum OpenHIDInterface(uint16_t vid, uint16_t pid, uint8_t reportId, HIDDevice* hid);
void CloseHIDInterface(HIDDevice hid);