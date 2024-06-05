#include "iap_config.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include "util/INIReader.h"
#include "util/utils.h"
#include "util/PEUtils.h"
#include "glad/glad.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#pragma pack(1)
struct Appendix
{
	uint32_t binAddr;
	uint32_t binSize;
	uint32_t vpidEntryNum;
	uint32_t vpidTableAddr;
	uint32_t colorEntryNum;
	uint32_t colorTableAddr;
	uint32_t resourceEntryNum;
	uint32_t resourceTableAddr;
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

std::filesystem::path selfName;

float iconTextureWidth;
float iconTextureHeight;
ImTextureID iconTextureID;
iap_config_t iap_config = {};


extern void* CreateTexture(uint8_t* data, int w, int h, char fmt);
extern void DeleteTexture(void* tex);


void LoadIconTexture()
{
	std::vector<uint8_t> self = utils::readFileData(selfName);

	int size = 128;

	std::vector<uint8_t> icon;
	LoadIconByPE(self.data(), icon, size);
	std::vector<uint8_t> dst = icon;

	size_t w = static_cast<size_t>(size);
	size_t h = static_cast<size_t>(size);
	for (size_t i = 0; i < h; ++i) {
		for (size_t j = 0; j < w; ++j) {
			size_t d = ((i * w) + j) * 4;
			size_t s = (((h - 1 - i) * w) + j) * 4;
			dst[d + 0] = icon[s + 2];
			dst[d + 1] = icon[s + 1];
			dst[d + 2] = icon[s + 0];
			dst[d + 3] = icon[s + 3];
		}
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst.data());

	iconTextureWidth = static_cast<float>(w);
	iconTextureHeight = static_cast<float>(h);
	iconTextureID = reinterpret_cast<void*>(static_cast<uintptr_t>(textureID));
}

static float takeout_float(uint8_t* buf)
{
	return *(float*)buf;
}
static uint32_t takeout_32_little(uint8_t* buf)
{
	return (buf[0]) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}
static uint32_t takeout_16_little(uint8_t* buf)
{
	return (buf[0]) | (buf[1] << 8);
}
static uint32_t takeout_8_little(uint8_t* buf)
{
	return (buf[0]);
}

bool LoadData()
{
	uint32_t size = (uint32_t)std::filesystem::file_size(selfName);
	printf("exe size:%d\n", size);

	std::vector<uint8_t> self = utils::readFileData(selfName);

	uint32_t N = 0;

	uint8_t* self_data = self.data();
	uint32_t self_size = (uint32_t)self.size();
	uint8_t* p = self_data;
	uint32_t k = self_size;
	k -= 4;
	if (takeout_32_little(&self_data[k]) == IAP_GEN_EXE_SUFFIX)
	{
		k -= 4;
		uint32_t N = takeout_32_little(&self_data[k]);

		Appendix* appendix = reinterpret_cast<Appendix*>(p + N);

		iap_config.retryNum = appendix->retryNum;
		iap_config.rebootDelay = appendix->rebootDelay;
		iap_config.switchDelay = appendix->switchDelay;
		iap_config.searchDeviceTimeout = appendix->searchDeviceTimeout;
		iap_config.readAckTimeout = appendix->readAckTimeout;

		iap_config.iap_bin.resize(appendix->binSize);
		memcpy(iap_config.iap_bin.data(), (p + appendix->binAddr), appendix->binSize);

		AppendixVPIDEntry* vpidEntryTable = reinterpret_cast<AppendixVPIDEntry*>(p + appendix->vpidTableAddr);
		iap_config.vpidEntryTable.resize(appendix->vpidEntryNum);
		for (uint32_t i = 0; i < appendix->vpidEntryNum; ++i) {
			AppendixVPIDEntry& vpidEntry = vpidEntryTable[i];
			iap_config.vpidEntryTable[i] = std::make_tuple(vpidEntry.vid, vpidEntry.pid, vpidEntry.rid);
		}

		AppendixColorEntry* colorEntryTable = reinterpret_cast<AppendixColorEntry*>(p + appendix->colorTableAddr);
		iap_config.color.resize(appendix->colorEntryNum);
		for (uint32_t i = 0; i < appendix->colorEntryNum; ++i) {
			iap_config.color[i] = colorEntryTable[i].color;
		}

		AppendixResourceEntry* resourceEntryTable = reinterpret_cast<AppendixResourceEntry*>(p + appendix->resourceTableAddr);
		for (uint32_t i = 0; i < appendix->resourceEntryNum; ++i) {
			AppendixResourceEntry& resourceEntry = resourceEntryTable[i];
			std::vector<uint8_t> buffer(resourceEntry.size);
			memcpy(buffer.data(), (p + resourceEntry.addr), resourceEntry.size);
			iap_config.gResourceMap[resourceEntry.key] = std::make_shared<ITexture>(buffer);
		}

		printf("retryNum:%d\n", iap_config.retryNum);
		printf("rebootDelay:%d\n", iap_config.rebootDelay);
		printf("switchDelay:%d\n", iap_config.switchDelay);
		printf("searchDeviceTimeout:%d\n", iap_config.searchDeviceTimeout);
		printf("readAckTimeout:%d\n", iap_config.readAckTimeout);
		
		// gResourceLayoutMap
		iap_config.gResourceLayoutMap[RSRC_IMG_BG]                  = std::make_tuple(ImVec2(  0.0f,   0.0f), ImVec2(600.0f, 400.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_LOGO]                = std::make_tuple(ImVec2(119.0f,  32.0f), ImVec2(119.0f, 118.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VID]      = std::make_tuple(ImVec2(410.0f,  50.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_PID]      = std::make_tuple(ImVec2(410.0f,  80.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VER]      = std::make_tuple(ImVec2(410.0f, 110.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_BOX]                 = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_BOX_CHECKED]         = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO]             = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_HOVERED]     = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_ACTIVE]      = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START]               = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START_HOVERED]       = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START_ACTIVE]        = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START]          = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_HOVERED]  = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_ACTIVE]   = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_VID]                 = std::make_tuple(ImVec2(350.0f,  56.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_PID]                 = std::make_tuple(ImVec2(350.0f,  86.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_VER]                 = std::make_tuple(ImVec2(350.0f, 116.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_PROGRESS]                = std::make_tuple(ImVec2( 56.0f, 182.0f), ImVec2(488.0f,  31.0f));
		iap_config.gResourceLayoutMap[RSRC_TIPS]                    = std::make_tuple(ImVec2(  0.0f, 220.0f), ImVec2(600.0f, 100.0f));

		return true;
	}
	else
	{
		iap_config.color.push_back({ ImVec4(0, 0, 0, 1.0f) });
		iap_config.color.push_back({ ImVec4(0.941176f, 0.941176f, 0.941176f, 1.0f) });
		iap_config.color.push_back({ ImVec4(0.258824f, 0.588235f, 0.980392f, 0.4f) });
		iap_config.color.push_back({ ImVec4(0.258824f, 0.588235f, 0.980392f, 1.0f) });
		iap_config.color.push_back({ ImVec4(0.0588235f, 0.529412f, 0.980392f, 1.0f) });
		iap_config.color.push_back({ ImVec4(0.901961f, 0.701961f, 0, 1.0f) });
		iap_config.color.push_back({ ImVec4(1.0f, 1.0f, 1.0f, 1.0f) });
		iap_config.color.push_back({ ImVec4(1.0f, 1.0f, 1.0f, 1.0f) });
		iap_config.color.push_back({ ImVec4(1.0f, 1.0f, 1.0f, 1.0f) });
		iap_config.color.push_back({ ImVec4(1.0f, 1.0f, 1.0f, 1.0f) });
		
		// gResourceLayoutMap
		iap_config.gResourceLayoutMap[RSRC_IMG_BG]                  = std::make_tuple(ImVec2(  0.0f,   0.0f), ImVec2(600.0f, 400.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_LOGO]                = std::make_tuple(ImVec2(119.0f,  32.0f), ImVec2(119.0f, 118.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VID]      = std::make_tuple(ImVec2(410.0f,  50.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_PID]      = std::make_tuple(ImVec2(410.0f,  80.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VER]      = std::make_tuple(ImVec2(410.0f, 110.0f), ImVec2( 74.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_BOX]                 = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_BOX_CHECKED]         = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO]             = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_HOVERED]     = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_ACTIVE]      = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START]               = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START_HOVERED]       = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_START_ACTIVE]        = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START]          = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_HOVERED]  = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_ACTIVE]   = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_VID]                 = std::make_tuple(ImVec2(350.0f,  56.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_PID]                 = std::make_tuple(ImVec2(350.0f,  86.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_IMG_VER]                 = std::make_tuple(ImVec2(350.0f, 116.0f), ImVec2( 46.0f,  13.0f));
		iap_config.gResourceLayoutMap[RSRC_PROGRESS]                = std::make_tuple(ImVec2( 56.0f, 182.0f), ImVec2(488.0f,  31.0f));
		iap_config.gResourceLayoutMap[RSRC_TIPS]                    = std::make_tuple(ImVec2(  0.0f, 220.0f), ImVec2(600.0f, 100.0f));

		return false;
	}
}

bool LoadData0()
{
	std::filesystem::path binName("C:\\Users\\leosh\\Desktop\\iap_release_1.2.0\\output\\INGIAP.bin");
	iap_config.iap_bin = utils::readFileData(binName);

	iap_config.retryNum = 3;
	iap_config.rebootDelay = 1000;
	iap_config.switchDelay = 1000;
	iap_config.searchDeviceTimeout = 8000;
	iap_config.readAckTimeout = 1000;

	// vpidEntryTable
	iap_config.vpidEntryTable.push_back(std::make_tuple(0x36B0, 0x3002, 0x3F));
	iap_config.vpidEntryTable.push_back(std::make_tuple(0x03EB, 0x2045, 0x3F));

	// colorEntryTable
	iap_config.color.resize(10);
	iap_config.color[0] = {ImVec4(0, 0, 0, 1.0f)};
	iap_config.color[1] = {ImVec4(0.941176f, 0.941176f, 0.941176f, 1.0f)};
	iap_config.color[2] = {ImVec4(0.258824f, 0.588235f, 0.980392f, 0.4f)};
	iap_config.color[3] = {ImVec4(0.258824f, 0.588235f, 0.980392f, 1.0f)};
	iap_config.color[4] = {ImVec4(0.0588235f, 0.529412f, 0.980392f, 1.0f)};
	iap_config.color[5] = {ImVec4(0.901961f, 0.701961f, 0, 1.0f)};
	iap_config.color[6] = {ImVec4(1.0f, 1.0f, 1.0f, 1.0f)};
	iap_config.color[7] = {ImVec4(1.0f, 1.0f, 1.0f, 1.0f)};
	iap_config.color[8] = {ImVec4(1.0f, 1.0f, 1.0f, 1.0f)};
	iap_config.color[9] = {ImVec4(1.0f, 1.0f, 1.0f, 1.0f)};

	// gResourceMap
	iap_config.gResourceMap[RSRC_IMG_BG]                 = std::make_shared<ITexture>("img/bg.bmp");
	iap_config.gResourceMap[RSRC_IMG_LOGO]               = std::make_shared<ITexture>("img/LOGO.bmp");
	iap_config.gResourceMap[RSRC_IMG_TEXT_INPUT_VID]     = std::make_shared<ITexture>("img/dev_info_box.bmp");
	iap_config.gResourceMap[RSRC_IMG_TEXT_INPUT_PID]     = std::make_shared<ITexture>("img/dev_info_box.bmp");
	iap_config.gResourceMap[RSRC_IMG_TEXT_INPUT_VER]     = std::make_shared<ITexture>("img/dev_info_box.bmp");
	iap_config.gResourceMap[RSRC_IMG_BOX]                = std::make_shared<ITexture>("img/auto_start_checkbox_no.bmp");
	iap_config.gResourceMap[RSRC_IMG_BOX_CHECKED]        = std::make_shared<ITexture>("img/auto_start_checkbox_yes.bmp");
	iap_config.gResourceMap[RSRC_IMG_DEVINFO]            = std::make_shared<ITexture>("img/Device info.bmp");
	iap_config.gResourceMap[RSRC_IMG_DEVINFO_HOVERED]    = std::make_shared<ITexture>("img/Device info hovered.bmp");
	iap_config.gResourceMap[RSRC_IMG_DEVINFO_ACTIVE]     = std::make_shared<ITexture>("img/Device info active.bmp");
	iap_config.gResourceMap[RSRC_IMG_START]              = std::make_shared<ITexture>("img/START.bmp");
	iap_config.gResourceMap[RSRC_IMG_START_HOVERED]      = std::make_shared<ITexture>("img/START HOVERED.bmp");
	iap_config.gResourceMap[RSRC_IMG_START_ACTIVE]       = std::make_shared<ITexture>("img/START ACTIVE.bmp");
	iap_config.gResourceMap[RSRC_IMG_AUTO_START]         = std::make_shared<ITexture>("img/AUTO START.bmp");
	iap_config.gResourceMap[RSRC_IMG_AUTO_START_HOVERED] = std::make_shared<ITexture>("img/AUTO START HOVERED.bmp");
	iap_config.gResourceMap[RSRC_IMG_AUTO_START_ACTIVE]  = std::make_shared<ITexture>("img/AUTO START ACTIVE.bmp");
	iap_config.gResourceMap[RSRC_IMG_PID]                = std::make_shared<ITexture>("img/PID.bmp");
	iap_config.gResourceMap[RSRC_IMG_VID]                = std::make_shared<ITexture>("img/VID.bmp");
	iap_config.gResourceMap[RSRC_IMG_VER]                = std::make_shared<ITexture>("img/VER.bmp");

	// gResourceLayoutMap
	iap_config.gResourceLayoutMap[RSRC_IMG_BG]                  = std::make_tuple(ImVec2(  0.0f,   0.0f), ImVec2(600.0f, 400.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_LOGO]                = std::make_tuple(ImVec2(119.0f,  32.0f), ImVec2(119.0f, 118.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VID]      = std::make_tuple(ImVec2(410.0f,  50.0f), ImVec2( 74.0f,  26.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_PID]      = std::make_tuple(ImVec2(410.0f,  80.0f), ImVec2( 74.0f,  26.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VER]      = std::make_tuple(ImVec2(410.0f, 110.0f), ImVec2( 74.0f,  26.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_BOX]                 = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_BOX_CHECKED]         = std::make_tuple(ImVec2(503.0f, 338.0f), ImVec2( 26.0f,  26.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO]             = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_HOVERED]     = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_DEVINFO_ACTIVE]      = std::make_tuple(ImVec2( 71.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_START]               = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_START_HOVERED]       = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_START_ACTIVE]        = std::make_tuple(ImVec2(248.0f, 332.0f), ImVec2( 66.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START]          = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_HOVERED]  = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_AUTO_START_ACTIVE]   = std::make_tuple(ImVec2(380.0f, 332.0f), ImVec2(111.0f,  38.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_VID]                 = std::make_tuple(ImVec2(350.0f,  56.0f), ImVec2( 46.0f,  13.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_PID]                 = std::make_tuple(ImVec2(350.0f,  86.0f), ImVec2( 46.0f,  13.0f));
	iap_config.gResourceLayoutMap[RSRC_IMG_VER]                 = std::make_tuple(ImVec2(350.0f, 116.0f), ImVec2( 46.0f,  13.0f));
	iap_config.gResourceLayoutMap[RSRC_PROGRESS]                = std::make_tuple(ImVec2( 56.0f, 182.0f), ImVec2(488.0f,  31.0f));
	iap_config.gResourceLayoutMap[RSRC_TIPS]                    = std::make_tuple(ImVec2(  0.0f, 220.0f), ImVec2(600.0f, 100.0f));

	return true;
}

std::string GetResourceKeyName(int key)
{
	switch (key)
	{
	case RSRC_IMG_BG:return "Background";
	case RSRC_IMG_LOGO:return "LOGO";
	case RSRC_IMG_TEXT_INPUT_VID:return "VID box";
	case RSRC_IMG_TEXT_INPUT_PID:return "PID box";
	case RSRC_IMG_TEXT_INPUT_VER:return "VER box";
	case RSRC_IMG_BOX:return "Auto start box";
	case RSRC_IMG_BOX_CHECKED:return "Auto start box Checked";
	case RSRC_IMG_DEVINFO:return "Device info button";
	case RSRC_IMG_DEVINFO_HOVERED:return "Device info button hovered";
	case RSRC_IMG_DEVINFO_ACTIVE:return "Device info button active";
	case RSRC_IMG_START:return "Start button";
	case RSRC_IMG_START_HOVERED:return "Start button hovered";
	case RSRC_IMG_START_ACTIVE:return "Start button active";
	case RSRC_IMG_AUTO_START:return "Auto start button";
	case RSRC_IMG_AUTO_START_HOVERED:return "Auto start button hovered";
	case RSRC_IMG_AUTO_START_ACTIVE:return "Auto start button active";
	case RSRC_IMG_VID:return "VID label";
	case RSRC_IMG_PID:return "PID label";
	case RSRC_IMG_VER:return "VER label";
	case RSRC_PROGRESS:return "progress";
	case RSRC_TIPS:return "tpis";
	default: return "None";
	};
}

#define EXE_CONFIG_DEFAULT_VID "36B0"
#define EXE_CONFIG_DEFAULT_PID "0101"
#define EXE_CONFIG_DEFAULT_RID "3F"

#define EXE_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT  8000
#define EXE_CONFIG_DEFAULT_READ_ACK_TIMEOUT 1000
#define EXE_CONFIG_DEFAULT_REBOOT_DELAY  1000
#define EXE_CONFIG_DEFAULT_SWITCH_DELAY  1000
#define EXE_CONFIG_DEFAULT_RETRY_NUM  3


static std::string colorDefault[COLORLIST_MAX_NUM] = {
	"0,0,0,1",
	"0.941176,0.941176,0.941176,1",
	"0.258824,0.588235,0.980392,0.4",
	"0.258824,0.588235,0.980392,1",
	"0.0588235,0.529412,0.980392,1",
	"0.901961,0.701961,0,1",
	"1,1,1,1",
	"1,1,1,1",
	"1,1,1,1",
	"1,1,1,1",
};

static int color_enums[COLORLIST_MAX_NUM] = {
	ImGuiCol_Text,
	ImGuiCol_WindowBg,
	ImGuiCol_Button,
	ImGuiCol_ButtonHovered,
	ImGuiCol_ButtonActive,
	ImGuiCol_PlotHistogram,
	ImGuiCol_FrameBg,
	ImGuiCol_COUNT,
	ImGuiCol_COUNT,
	ImGuiCol_COUNT,
};

std::vector<int> rsrc_enums = {
	RSRC_IMG_BG,
	RSRC_IMG_LOGO,
	RSRC_IMG_TEXT_INPUT_VID,
	RSRC_IMG_TEXT_INPUT_PID,
	RSRC_IMG_TEXT_INPUT_VER,
	RSRC_IMG_BOX,
	RSRC_IMG_BOX_CHECKED,
	RSRC_IMG_DEVINFO,
	RSRC_IMG_DEVINFO_HOVERED,
	RSRC_IMG_DEVINFO_ACTIVE,
	RSRC_IMG_START,
	RSRC_IMG_START_HOVERED,
	RSRC_IMG_START_ACTIVE,
	RSRC_IMG_AUTO_START,
	RSRC_IMG_AUTO_START_HOVERED,
	RSRC_IMG_AUTO_START_ACTIVE,
	RSRC_IMG_VID,
	RSRC_IMG_PID,
	RSRC_IMG_VER,
};

std::vector<int>& get_rsrc_enums()
{
	return rsrc_enums;
}

ImVec4 loadImVec4FromString(std::string s)
{
	ImVec4 r;
	std::stringstream ss(s);
	char delimiter = ',';
	std::string token;
	std::getline(ss, token, delimiter);
	r.x = std::stof(token);
	std::getline(ss, token, delimiter);
	r.y = std::stof(token);
	std::getline(ss, token, delimiter);
	r.z = std::stof(token);
	std::getline(ss, token, delimiter);
	r.w = std::stof(token);
	return r;
}

void SaveEXEConfig(exe_config_ui_t& c, std::filesystem::path ini_path)
{
	std::ofstream out;

	out.open(ini_path, std::ios::out);
	out << "[option]" << std::endl;
	out << "retryNum=" << c.retryNum << std::endl;
	out << "rebootDelay=" << c.rebootDelay << std::endl;
	out << "switchDelay=" << c.switchDelay << std::endl;
	out << "searchDeviceTimeout=" << c.searchDeviceTimeout << std::endl;
	out << "readAckTimeout=" << c.readAckTimeout << std::endl;
	out << "vpridsNum=" << c.vpridsNum << std::endl;
	out << std::endl;

	for (int i = 0; i < c.vpridsNum; ++i) {
		out << "[VPR-" << i << "]" << std::endl;
		out << "checked=" << c.vprids[i].checked << std::endl;
		out << "vid=" << c.vprids[i].tvid << std::endl;
		out << "pid=" << c.vprids[i].tpid << std::endl;
		out << "rid=" << c.vprids[i].trid << std::endl;
		out << std::endl;
	}

	out << "[COLOR]" << std::endl;
	for (int i = 0; i < c.colorsNum; ++i) {
		out << "color" << i << "=" 
			<< c.colors[i].colVO.x << ","
			<< c.colors[i].colVO.y << "," 
			<< c.colors[i].colVO.z << "," 
			<< c.colors[i].colVO.w << std::endl;
		out << std::endl;
	}

	for (int i = 0; i < rsrc_enums.size(); ++i) {
		out << "[RSRC-" << rsrc_enums[i] << "]" << std::endl;
		out << "key=" << c.rsrcs[i].key << std::endl;
		out << "name=" << c.rsrcs[i].name_gbk << std::endl;
		out << std::endl;
	}

	out << "[style]" << std::endl;
	out << "icon_name=" << c.icon.name_gbk << std::endl;
	out << std::endl;

	out << std::endl;
	out.close();
}

void LoadEXEConfig(exe_config_ui_t& c, std::filesystem::path ini_path)
{
#define NAME_MAX 32
	char name[NAME_MAX];

	INIReader reader(ini_path.generic_string());

	c.retryNum = reader.GetInteger("option", "retryNum", EXE_CONFIG_DEFAULT_RETRY_NUM);
	c.rebootDelay = reader.GetInteger("option", "rebootDelay", EXE_CONFIG_DEFAULT_REBOOT_DELAY);
	c.switchDelay = reader.GetInteger("option", "switchDelay", EXE_CONFIG_DEFAULT_SWITCH_DELAY);
	c.searchDeviceTimeout = reader.GetInteger("option", "searchDeviceTimeout", EXE_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT);
	c.readAckTimeout = reader.GetInteger("option", "readAckTimeout", EXE_CONFIG_DEFAULT_READ_ACK_TIMEOUT);
	
	c.vpridsNum = VPIDLIST_MAX_NUM;
	for (uint32_t i = 0; i < VPIDLIST_MAX_NUM; ++i) {
		sprintf_s(name, NAME_MAX, "VPR-%d", i);
		c.vprids[i].checked = reader.GetBoolean(name, "checked", false);
		strcpy(c.vprids[i].tvid, reader.Get(name, "vid", EXE_CONFIG_DEFAULT_VID).c_str());
		strcpy(c.vprids[i].tpid, reader.Get(name, "pid", EXE_CONFIG_DEFAULT_PID).c_str());
		strcpy(c.vprids[i].trid, reader.Get(name, "rid", EXE_CONFIG_DEFAULT_RID).c_str());
	}

	c.colorsNum = COLORLIST_MAX_NUM;
	for (uint32_t i = 0; i < COLORLIST_MAX_NUM; ++i) {
		sprintf_s(name, NAME_MAX, "color%d", i);
		c.colors[i].colID = color_enums[i];
		c.colors[i].colVO = c.colors[i].colVE = loadImVec4FromString(reader.Get("COLOR", name, colorDefault[i]));
	}

	c.rsrcsNum = static_cast<int>(rsrc_enums.size());
	for (uint32_t i = 0; i < rsrc_enums.size(); ++i) {
		sprintf_s(name, NAME_MAX, "RSRC-%d", rsrc_enums[i]);
		c.rsrcs[i].key = reader.GetInteger(name, "key", rsrc_enums[i]);
		strcpy(c.rsrcs[i].name_gbk, reader.Get(name, "name", "").c_str());
		strcpy(c.rsrcs[i].name, utils::gbk_to_utf8(c.rsrcs[i].name_gbk).c_str());
	}

	strcpy(c.icon.name_gbk, reader.Get("style", "icon_name", "icon.png").c_str());
	strcpy(c.icon.name, utils::gbk_to_utf8(c.icon.name_gbk).c_str());
}

uint32_t GetValidVPRIDEntryNum(exe_config_ui_t& c)
{
	uint32_t sum = 0;
	for (int i = 0; i < c.vpridsNum; ++i)
		if (c.vprids[i].checked)
			sum++;
	return sum;
}

uint32_t CalcEXEDataSize(exe_config_ui_t& c, std::vector<uint8_t>& exe_template, std::vector<uint8_t>& bin)
{
	uint32_t r = 0;
	r += static_cast<uint32_t>(exe_template.size());
	r += static_cast<uint32_t>(bin.size());
	r += 8;	// Bin Struct
	r += sizeof(Appendix);
	r += GetValidVPRIDEntryNum(c) * sizeof(AppendixVPIDEntry);
	r += c.colorsNum * sizeof(AppendixColorEntry);
	r += c.rsrcsNum * sizeof(AppendixResourceEntry);
	for (int i = 0; i < c.rsrcsNum; ++i)
		r += c.rsrcs[i].size;
	return r;
}

void append_32_little(uint32_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
	outbuf[1] = v >> 8;
	outbuf[2] = v >> 16;
	outbuf[3] = v >> 24;
}
void append_16_little(uint16_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
	outbuf[1] = v >> 8;
}
void append_8_little(uint16_t v, uint8_t* outbuf)
{
	outbuf[0] = v >> 0;
}

void ExportEXEData(exe_config_ui_t& c, std::vector<uint8_t>& exe_template, std::vector<uint8_t>& bin, std::vector<uint8_t>& out)
{
	uint32_t suffix = IAP_GEN_EXE_SUFFIX;

	uint32_t totalSize = CalcEXEDataSize(c, exe_template, bin);
	out.resize(totalSize);

	uint8_t* p = out.data();
	uint32_t k = 0, l = 0;

	memcpy(p + k, exe_template.data(), exe_template.size());
	k += static_cast<uint32_t>(exe_template.size());
	l += static_cast<uint32_t>(exe_template.size());

	Appendix appendix = {};
	appendix.binAddr = 0xFFFFFFFF;
	appendix.binSize = static_cast<uint32_t>(bin.size());
	appendix.vpidEntryNum = GetValidVPRIDEntryNum(c);
	appendix.vpidTableAddr = 0xFFFFFFFF;
	appendix.colorEntryNum = c.colorsNum;
	appendix.colorTableAddr = 0xFFFFFFFF;
	appendix.resourceEntryNum = c.rsrcsNum;
	appendix.resourceTableAddr = 0xFFFFFFFF;
	appendix.retryNum = c.retryNum;
	appendix.rebootDelay = c.rebootDelay;
	appendix.switchDelay = c.switchDelay;
	appendix.searchDeviceTimeout = c.searchDeviceTimeout;
	appendix.readAckTimeout = c.readAckTimeout;

	l += sizeof(Appendix);
	appendix.binAddr = l;
	l += static_cast<uint32_t>(bin.size());
	appendix.vpidTableAddr = l;
	l += appendix.vpidEntryNum * sizeof(AppendixVPIDEntry);
	appendix.colorTableAddr = l;
	l += appendix.colorEntryNum * sizeof(AppendixColorEntry);
	appendix.resourceTableAddr = l;
	l += appendix.resourceEntryNum * sizeof(AppendixResourceEntry);

	memcpy(p + k, &appendix, sizeof(Appendix));
	k += sizeof(Appendix);

	memcpy(p + k, bin.data(), bin.size());
	k += static_cast<uint32_t>(bin.size());

	for (int i = 0; i < c.vpridsNum; ++i) {
		if (!c.vprids[i].checked)
			continue;
		AppendixVPIDEntry entry = {};
		entry.vid = c.vprids[i].vid;
		entry.pid = c.vprids[i].pid;
		entry.rid = c.vprids[i].rid;
		memcpy(p + k, &entry, sizeof(AppendixVPIDEntry));
		k += sizeof(AppendixVPIDEntry);
	}

	for (int i = 0; i < c.colorsNum; ++i) {
		AppendixColorEntry entry = {};
		entry.color = c.colors[i].colVO;
		memcpy(p + k, &entry, sizeof(AppendixColorEntry));
		k += sizeof(AppendixColorEntry);
	}

	for (int i = 0; i < c.rsrcsNum; ++i) {
		AppendixResourceEntry entry = {};
		entry.key = c.rsrcs[i].key;
		entry.size = static_cast<uint32_t>(c.rsrcs[i].buffer.size());
		entry.addr = l;
		memcpy(p + k, &entry, sizeof(AppendixResourceEntry));
		k += sizeof(AppendixResourceEntry);

		l += entry.size;
	}

	for (int i = 0; i < c.rsrcsNum; ++i) {
		memcpy(p + k, c.rsrcs[i].buffer.data(), c.rsrcs[i].buffer.size());
		k += static_cast<uint32_t>(c.rsrcs[i].buffer.size());
	}

	append_32_little(static_cast<uint32_t>(exe_template.size()), p + k);
	k += 4;
	append_32_little(suffix, p + k);
	k += 4;

	assert(out.size() == k);
}