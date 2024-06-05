#pragma once

#include "pch.h"

#include <filesystem>
#include <unordered_map>
#include <tuple>
#include <vector>
#include "imgui/imgui.h"

#include "itexture.h"


#define VPIDLIST_MAX_NUM 10
#define COLORLIST_MAX_NUM 10
#define RSRC_LIST_MAX_NUM 21

enum
{
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
	RSRC_PROGRESS,
	RSRC_TIPS,
};


struct VPRID_ui_t
{
	uint16_t vid;
	uint16_t pid;
	uint8_t  rid;
	bool checked;
	char tvid[5];
	char tpid[5];
	char trid[3];
};

struct color_ui_t
{
	ImGuiCol colID;
	ImVec4 colVO;
	ImVec4 colVE;
};

struct rsrc_ui_t
{
	int key;
	char name[BIN_NAME_BUFF_MAX_SIZE];
	char name_gbk[BIN_NAME_BUFF_MAX_SIZE];
	uint32_t size;
	std::vector<uint8_t> buffer;

	ImTextureID texID;
	ImVec2 texSize;
};

struct exe_config_ui_t
{
	int retryNum;
	int rebootDelay;
	int switchDelay;
	int searchDeviceTimeout;
	int readAckTimeout;

	int vpridsNum;
	VPRID_ui_t vprids[VPIDLIST_MAX_NUM];

	int colorsNum;
	color_ui_t colors[COLORLIST_MAX_NUM];

	int rsrcsNum;
	rsrc_ui_t rsrcs[RSRC_LIST_MAX_NUM];

	rsrc_ui_t icon;
};

struct iap_config_t
{
	int retryNum;
	int rebootDelay;
	int switchDelay;
	int searchDeviceTimeout;
	int readAckTimeout;

	std::vector<std::tuple<uint16_t, uint16_t, uint8_t>> vpidEntryTable;
	std::vector<ImVec4> color;

	std::vector<uint8_t> iap_bin;
	std::unordered_map<int, std::shared_ptr<ITexture>> gResourceMap;
	std::unordered_map<int, std::tuple<ImVec2, ImVec2>> gResourceLayoutMap;
};

void LoadIconTexture();

bool LoadData0();
bool LoadData();

std::vector<int>& get_rsrc_enums();
std::string GetResourceKeyName(int key);

void SaveEXEConfig(exe_config_ui_t& c, std::filesystem::path ini_path);
void LoadEXEConfig(exe_config_ui_t& c, std::filesystem::path ini_path);

void ExportEXEData(exe_config_ui_t& c, std::vector<uint8_t>& exe_template, std::vector<uint8_t>& bin, std::vector<uint8_t>& out);
