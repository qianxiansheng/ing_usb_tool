#pragma once

#include <iostream>
#include <vector>
#include <tuple>

#include "hidapi.h"
#include "libusb-1.0/libusb.h"


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
	uint16_t usage;
};

bool HIDInit();
void HIDExit();
HIDFindResultEnum OpenHIDInterface(std::vector<std::tuple<uint16_t, uint16_t, uint8_t>>& vpridList, HIDDevice* hid);
void CloseHIDInterface(HIDDevice hid);
bool GetUSBDeviceInfo(std::vector<std::tuple<uint16_t, uint16_t, uint8_t>>& vpridList, uint16_t* VID, uint16_t* PID, uint16_t* bcdDevice);