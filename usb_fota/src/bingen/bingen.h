#pragma once

#include "pch.h"

#include <vector>
#include "imgui/imgui.h"

#pragma pack(1)
struct Appendix
{
	uint32_t binAddr;
	uint32_t binSize;
	uint32_t vpidEntryNum;
	uint32_t vpidTableAddr;
	uint32_t resourceEntryNum;
	uint32_t resourceTableAddr;
	uint32_t colorEntryNum;
	uint32_t colorTableAddr;
	uint32_t retryNum;
	uint32_t rebootDelay;
	uint32_t switchDelay;
	uint32_t searchDeviceTimeout;
	uint32_t readAckTimeout;
};

struct AppendixVPIDEntry
{
	uint16_t vid;
	uint16_t pid;
	uint8_t  rid;
};
struct AppendixResourceEntry
{
	uint32_t key;
	uint32_t size;
	uint32_t addr;
};
struct AppendixColorEntry
{
	ImVec4 color;
};
struct AppendixResourceData
{
	std::vector<uint8_t> buff;
};
#pragma pack()

void ShowBinGenWindow(bool* p_open);

void LoadBinGenINI();
void SaveBinGenINI();