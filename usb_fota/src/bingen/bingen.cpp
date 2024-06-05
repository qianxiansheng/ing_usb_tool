#include "bingen.h"

#include "bin.h"
#include "iap_config.h"

#include <sstream>
#include <fstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "util/utils.h"

#include "imgui/extensions/ImConsole.h"
#include "imgui/extensions/ImFileDialog.h"

#include "./bingen/template.cpp"

#include "util/PEUtils.h"
#include "setting.h"

#include "org/qxs/bmp/PNGLoader.hpp"
#include "org/qxs/bmp/BMPLoader.hpp"
#include "org/qxs/bmp/scaler/Scaler.hpp"
#include "org/qxs/bmp/scaler/BilinearInterpolatorScaler.hpp"
#include "org/qxs/PE/rsrc/ResourceDataDirBuilder.hpp"
#include "org/qxs/PE/PEFile.hpp"
#include "org/qxs/PE/PELoader.hpp"
#include "org/qxs/PE/PEConverter.hpp"
#include "org/qxs/PE/IconTool.hpp"

extern ImLogger* logger;
extern bin_config_t bin_config;
extern uint8_t* p_mem;
extern size_t s_mem;
extern app_settings_t gSettings;

static std::filesystem::path work_dir     = std::filesystem::current_path();
static std::filesystem::path ini_path     = std::filesystem::current_path().concat("\\bin_config.ini");
static std::filesystem::path ini_path_exe = std::filesystem::current_path().concat("\\exe_config.ini");
static std::filesystem::path export_path;

std::filesystem::path export_dir;

static bool alert_flag = false;
static char alert_message[256] = { 0 };

exe_config_ui_t exe_config;

void Alert(std::string message)
{
	alert_flag = true;
	strcpy(alert_message, message.c_str());
}

void CalcCRC()
{

}

bool IsMergeable()
{
	auto& c = bin_config;
	return (c.upgrade_type == upgrade_type_e::UPGRADE_TYPE_PLATFORM_APP && (c.platform.size != 0 && c.app.size != 0)) ||
		   (c.upgrade_type == upgrade_type_e::UPGRADE_TYPE_APP_ONLY && c.app.size != 0);
}

void Mergebin()
{
	auto& c = bin_config;
	if (!IsMergeable()) {
		// Some bin files do not exist
		c.out_data.resize(0);
		return;
	}

	switch (c.upgrade_type)
	{
	case upgrade_type_e::UPGRADE_TYPE_PLATFORM_APP:
	{
		c.out_data_load_addr = c.platform.load_addr;
		uint32_t app_relative_address = c.app.load_addr - c.out_data_load_addr;
		uint32_t out_data_size = app_relative_address + static_cast<uint32_t>(c.app.data.size());
		c.out_data.resize(out_data_size);
		memset(c.out_data.data(), 0xFF, c.out_data.size());
		memcpy(c.out_data.data(), c.platform.data.data(), c.platform.data.size());
		memcpy(c.out_data.data() + app_relative_address, c.app.data.data(), c.app.data.size());
		c.out_data_crc = utils::crc16_modbus(c.out_data.data(), static_cast<uint32_t>(c.out_data.size()));
	}
	break;
	case upgrade_type_e::UPGRADE_TYPE_APP_ONLY:
	default:
	{
		c.out_data_load_addr = c.app.load_addr;
		c.out_data.resize(c.app.data.size());
		memcpy(c.out_data.data(), c.app.data.data(), c.app.data.size());
		c.out_data_crc = utils::crc16_modbus(c.out_data.data(), static_cast<uint32_t>(c.out_data.size()));
	}
	break;
	}
}

static void ReloadPlatform()
{
	auto& c = bin_config;

	int align_16_size = ((c.platform.size + 15) & ~(15));
	c.platform.data.resize(align_16_size);
	memset(c.platform.data.data(), 0xFF, align_16_size);
	utils::readFileData(c.platform.name_gbk, c.platform.data.data());
}

static void ReloadApp()
{
	auto& c = bin_config;

	int align_16_size = ((c.app.size + 15) & ~(15));
	c.app.data.resize(align_16_size);
	memset(c.app.data.data(), 0xFF, align_16_size);
	utils::readFileData(c.app.name_gbk, c.app.data.data());
}

extern void* CreateTexture(uint8_t* data, int w, int h, char fmt);
extern void DeleteTexture(void* tex);


static bool ResourceIsBMP(uint8_t* header)
{
	return (header[0] == 'B' && header[1] == 'M');
}

static bool ResourceIsPNG(uint8_t* header)
{
	return (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G' &&
		header[4] == '\r' && header[5] == '\n' && header[6] == 0x1A && header[7] == '\n');
}

static void ResourceLoadTextureBMP(rsrc_ui_t& rsrc)
{
	if (rsrc.texID) {
		DeleteTexture(rsrc.texID);
		rsrc.texID = NULL;
	}

	org::qxs::bmp::BMPLoader loader;
	org::qxs::bmp::scaler::BilinearInterpolatorScaler scaler;
	auto bitmap = loader.load_image(rsrc.name_gbk);

	rsrc.texSize.x = static_cast<float>(bitmap->_bitmaps[0]._w);
	rsrc.texSize.y = static_cast<float>(bitmap->_bitmaps[0]._h);
	rsrc.texID = (ImTextureID)CreateTexture(bitmap->_bitmaps[0]._d.data(), bitmap->_bitmaps[0]._w, bitmap->_bitmaps[0]._h, 1);
}

static void ResourceLoadTexturePNG(rsrc_ui_t& rsrc)
{
	if (rsrc.texID) {
		DeleteTexture(rsrc.texID);
		rsrc.texID = NULL;
	}

	org::qxs::bmp::PNGLoader loader;
	org::qxs::bmp::scaler::BilinearInterpolatorScaler scaler;
	auto bitmap = loader.load_image(rsrc.name_gbk);

	rsrc.texSize.x = static_cast<float>(bitmap->_bitmaps[0]._w);
	rsrc.texSize.y = static_cast<float>(bitmap->_bitmaps[0]._h);
	rsrc.texID = (ImTextureID)CreateTexture(bitmap->_bitmaps[0]._d.data(), bitmap->_bitmaps[0]._w, bitmap->_bitmaps[0]._h, 1);
}

void ResourceLoadTexture(rsrc_ui_t& rsrc)
{
	if (ResourceIsBMP(rsrc.buffer.data())) {
		ResourceLoadTextureBMP(rsrc);
	}
	else if (ResourceIsPNG(rsrc.buffer.data())) {
		ResourceLoadTexturePNG(rsrc);
	}
	else {
		throw std::exception("Only support images in .png and .bmp formats.");
	}
}

void ResourceLoadDataAndTexture(rsrc_ui_t& rsrc)
{
	std::filesystem::path path = rsrc.name_gbk;
	rsrc.size = std::filesystem::is_regular_file(path) ?
		static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
	if (rsrc.size != 0) {
		rsrc.buffer.resize(rsrc.size);
		utils::readFileData(rsrc.name_gbk, rsrc.buffer.data());
		ResourceLoadTexture(rsrc);
	}
}


#define FORMAT_RGBA_GL                           0x1908

static std::string ExportFileName0()
{
	return "INGIAP.bin";
}
static std::string ExportFileName()
{
	auto& c = bin_config;

	char buf[32] = { 0 };

	std::stringstream ss;
	ss << "INGIAP_";
	if (strlen(c.proj_code) <= 5)
		ss << c.proj_code << '_';
	else
	{
		memcpy(buf, c.proj_code, 5);
		buf[5] = '\0';
		ss << buf << "_";
	}
	ss << "HW" << c.hard_version[1] << "_" << c.hard_version[3] << "_" << c.hard_version[5] << "_";
	ss << "SW" << c.soft_version[1] << "_" << c.soft_version[3] << "_" << c.soft_version[5] << "_";

	if (c.check_type == CHECK_TYPE_CRC)
		ss << "CRC_";
	else if (c.check_type == CHECK_TYPE_SUM)
		ss << "SUM_";

	if (c.upgrade_type == UPGRADE_TYPE_APP_ONLY)
		ss << "A_";
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_APP)
		ss << "PA_";

	if (c.encryption_enable)
	{
		if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
			ss << "Y0_";
		else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
			ss << "Y1_";
		else
			ss << "X_";
	}
	else
	{
		ss << "N_";
	}

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
	auto as_time_t = std::chrono::system_clock::to_time_t(t);
	struct tm tm = utils::time_to_tm(as_time_t);

	sprintf(buf, "%04d%02d%02d_%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

	ss << buf;
	ss << ".bin";

	return ss.str();
}
static std::filesystem::path DefaultExportDir()
{
	return std::filesystem::path(work_dir).concat("\\output\\");
}


static bool GenerateBin()
{
	if (!std::filesystem::exists(export_dir))
		std::filesystem::create_directory(export_dir);

	export_path = std::filesystem::path(export_dir).concat(ExportFileName());

	/* Reload File Binary Data */
	{
		auto& c = bin_config;
		if (c.upgrade_type == upgrade_type_e::UPGRADE_TYPE_PLATFORM_APP) {
			auto path = c.platform.name_gbk;
			uint32_t size = std::filesystem::is_regular_file(path) ?
				static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
			if (size != 0) {
				ReloadPlatform();
			}
		}
		{
			auto path = c.app.name_gbk;
			uint32_t size = std::filesystem::is_regular_file(path) ?
				static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
			if (size != 0) {
				ReloadApp();
			}
		}
		Mergebin();

		size_t block_num = (c.out_data.size() - 1) / static_cast<size_t>(c.block_size) + 1;
		c.block_num = static_cast<uint16_t>(block_num);
	}

	/* Check Resource */
	std::unordered_map<int, std::string> rsrc_enum_not_ready;
	auto& rsrc_enums = get_rsrc_enums();
	for (const auto& rsrc_key : rsrc_enums) {
		bool check_ok = false;
		std::string name_utf8;
		for (const auto& rsrc : exe_config.rsrcs) {
			if (rsrc.key == rsrc_key) {
				if (std::filesystem::is_regular_file(rsrc.name_gbk) && rsrc.texID != NULL) {
					check_ok = true;
				} else {
					name_utf8 = rsrc.name;
				}
				break;
			}
		}
		if (!check_ok) {
			rsrc_enum_not_ready[rsrc_key] = name_utf8;
		}
	}
	if (rsrc_enum_not_ready.size() > 0) {
		std::stringstream ss;
		ss << "The following resources failed to load." << std::endl;
		for (auto& entry : rsrc_enum_not_ready) {
			std::string r = GetResourceKeyName(entry.first);
			ss << "| " << r << "  -  " << entry.second << std::string(32 - entry.second.length(), ' ') << std::endl;
		}
		Alert(ss.str().c_str());
		return false;
	}

	/* Generate firmware bin */
	Exportbin(export_path);

	uint64_t size = std::filesystem::file_size(export_path);
	bin_config.out_data.resize(size);
	utils::readFileData(export_path, bin_config.out_data.data());

	/* Binary viewer */
	p_mem = bin_config.out_data.data();
	s_mem = bin_config.out_data.size();
	logger->AddLog("[Export] %s\n", utils::gbk_to_utf8(export_path.generic_string()).c_str());

	/* Exe Template */
	std::vector<uint8_t> exeTemplate(sizeof(template_data));
	memcpy(exeTemplate.data(), template_data, exeTemplate.size());

	/* Replace Icon */
	if (std::filesystem::exists(exe_config.icon.name_gbk))
	{
		org::qxs::bmp::PNGLoader loader;
		org::qxs::bmp::scaler::BilinearInterpolatorScaler scaler;

		auto bitmap = loader.load_image(exe_config.icon.name_gbk);
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 16, 16));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 24, 24));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 32, 32));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 48, 48));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 64, 64));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 72, 72));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 80, 80));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 96, 96));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 128, 128));
		bitmap->_bitmaps.push_back(scaler.scaler(bitmap->_bitmaps[0], 256, 256));
		bitmap->_bitmaps.erase(bitmap->_bitmaps.begin());

		auto resourceDataDir = org::qxs::pe::rsrc::ResourceDataDirBuilder()
			.setIcon(*bitmap)
			.build();

		auto peFile = org::qxs::pe::PELoader::loadFile(exeTemplate);
		org::qxs::pe::IconTool::replaceIcon(peFile, resourceDataDir._rsrc_data_dir);
		exeTemplate = org::qxs::pe::PEConverter::convertFile(peFile);
	}

	/* Export EXE */
	std::vector<uint8_t> exeOut;
	ExportEXEData(exe_config, exeTemplate, bin_config.out_data, exeOut);

	auto exepath = std::filesystem::path(export_dir).concat("iap.exe");
	std::ofstream ofs;
	ofs.open(exepath, std::ios::out | std::ios::binary);
	if (!ofs) {
		logger->AddLog("[Info] Failed to gen exe(File In Use) %s\n", utils::gbk_to_utf8(exepath.generic_string()).c_str());
		Alert("Export failed because the file is in use.");
		return false;
	} else {
		ofs.write((const char*)exeOut.data(), exeOut.size());
		ofs.flush();
		ofs.close();
		logger->AddLog("[Gen exe] %s\n", utils::gbk_to_utf8(exepath.generic_string()).c_str());
		return true;
	}
}

static void AnalysisBin()
{
	Alsbin(export_path);
}

static void ShowSubTitle(const char* title)
{
	ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();
	float x = ImGui::CalcTextSize(title).x;
	ImGuiWindow* window = ImGui::GetCurrentWindow();

	ImGuiDCYMargin(10);

	float item_top_y = window->DC.CursorPos.y;
	ImGui::Text(title);
	float item_bottom_y = window->DC.CursorPos.y;
	float item_y_half = (item_bottom_y - item_top_y) / 2;

	window->DrawList->AddRectFilled(
		ImVec2(window->ContentRegionRect.Min.x + x + 5, item_top_y + item_y_half - 0.5f),
		ImVec2(window->ContentRegionRect.Min.x + contentRegionAvail.x, item_bottom_y - item_y_half + 0.5f),
		0xFFB6B6B6
	);

	ImGuiDCYMargin(10);
}

bool FileInput(int id, char* buffer, char* buffer_gbk, int buf_size, uint32_t& data_size, std::vector<uint8_t>& data)
{
	ImGui::PushID(id);
	char key[32];
	sprintf_s(key, 32, "ICON_OPEN_%d", id);

	char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
	strcpy(bin, buffer);
	ImGui::InputTextEx("##FILE_NAME", "", buffer, buf_size, ImVec2(0.0f, 0.0f), 0);
	ImGui::SameLine();
	if (ImGui::Button("Open##FILE_OPEN_BTN"))
		ifd::FileDialog::Instance().Open(key, "Choose bin", "image {.png,.bmp}", false);
	if (ifd::FileDialog::Instance().IsDone(key)) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			if (res.size() > 0) {
				std::filesystem::path path = res[0];
				std::string pathStr = path.u8string();
				strcpy(buffer, pathStr.c_str());
			}
		}
		ifd::FileDialog::Instance().Close();
	}
	if (buffer[0] != '\0')
	{
		ImGui::SameLine();
		if (data_size == 0) {
			ImGui::Text("Error: file not exists");
		}
		else {
			ImGui::Text("OK. file size = %dB", data_size);
		}
	}
	strcpy(buffer_gbk, utils::utf8_to_gbk(buffer).c_str());
	auto path = buffer_gbk;
	data_size = std::filesystem::is_regular_file(path) ?
		static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
	bool r = false;
	if (data_size != 0 && (data.size() == 0 || strcmp(bin, buffer) != 0)) {
		data.resize(data_size);
		utils::readFileData(buffer_gbk, data.data());
		r = true;
	}
	ImGui::PopID();
	return r;
}

#ifndef IMGUI_DEFINE_MATH_OPERATORS
static ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
	return ImVec2(a.x + b.x, a.y + b.y);
}
#endif
void ShowBinGenWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("Bin Editor", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	app_settings_t& s = gSettings;

	const int cl = 200;
	const float wv = 300.0f;
	char buf[7] = { 0 };
	auto& c = bin_config;

	struct combo_item
	{
		const char name[32];
		int value;
	};

	const combo_item check_type_items[] = {
		{"CRC", CHECK_TYPE_CRC},
		{"SUM", CHECK_TYPE_SUM}
	};

	const combo_item upgrade_type_items[] = {
		{"APP ONLY", UPGRADE_TYPE_APP_ONLY},
		{"PLATFORM + APP", UPGRADE_TYPE_PLATFORM_APP},
	};

	const combo_item encryption_type_items[] = {
		{"XOR", BIN_ENCRYPTION_TYPE_XOR},
		{"AES", BIN_ENCRYPTION_TYPE_AES}
	};

	static int check_type_idx = 0;
	static int upgrade_type_idx = 0;
	static int encryption_type_idx = 0;

	if (c.check_type == CHECK_TYPE_CRC)							check_type_idx = 0;
	else if (c.check_type == CHECK_TYPE_SUM)					check_type_idx = 1;
	else														check_type_idx = 0;

	if (c.upgrade_type == UPGRADE_TYPE_APP_ONLY)				upgrade_type_idx = 0;
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_APP)		upgrade_type_idx = 1;
	else														upgrade_type_idx = 0;

	if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)			encryption_type_idx = 0;
	else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)		encryption_type_idx = 1;
	else														encryption_type_idx = 1;

	const char* check_type_value = check_type_items[check_type_idx].name;
	const char* upgrade_type_value = upgrade_type_items[upgrade_type_idx].name;
	const char* encryption_type_value = encryption_type_items[encryption_type_idx].name;


	if (s.opt_bingen_base_show && ImGui::CollapsingHeader("Base", s.opt_bingen_base_expand ? ImGuiTreeNodeFlags_DefaultOpen : 0))
	{
		if (s.opt_bingen_base_chipcode_show)
		{
			ImGui::Text("Chip code");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			ImGui::InputTextEx("##CHIP_CODE", "", c.chip_code, sizeof(c.chip_code), ImVec2(300.0f, 0.0f), 0);
		}
		if (s.opt_bingen_base_hardversion_show)
		{
			ImGui::Text("Project code");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			ImGui::InputTextEx("##PROJ_CODE", "", c.proj_code, sizeof(c.proj_code), ImVec2(300.0f, 0.0f), 0);
		}
		strcpy(buf, c.hard_version);
		strcpy(buf, c.soft_version);
		if (s.opt_bingen_base_hardversion_show)
		{
			ImGui::Text("Hardware version");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			ImGui::InputTextEx("##HARD_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
			ImGui::SameLine();
			utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");
		}
		if (s.opt_bingen_base_softversion_show)
		{
			ImGui::Text("Software version");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			ImGui::InputTextEx("##SOFT_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
			ImGui::SameLine();
			utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");
		}
		if (utils::ValidateVersion(buf))	strcpy(c.hard_version, buf);
		if (utils::ValidateVersion(buf))	strcpy(c.soft_version, buf);
		
		if (s.opt_bingen_base_blocksize_show)
		{
			ImGui::Text("Block Size");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			ImGui::InputScalar("##BLOCK_SIZE", ImGuiDataType_U16, &c.block_size, NULL, "%u");
			ImGui::SameLine();
			utils::HelpMarker("Range: between 12 and 8192 bytes.");
		}
		if (c.block_size < 12) c.block_size = 12;
		if (c.block_size > 8192) c.block_size = 8192;

		if (s.opt_bingen_base_upgradetype_show)
		{
			ImGui::Text("Upgrate Type");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			if (ImGui::BeginCombo("##UPGRADE_TYPE", upgrade_type_value, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(upgrade_type_items); n++)
				{
					const bool is_selected = (upgrade_type_idx == n);
					if (ImGui::Selectable(upgrade_type_items[n].name, is_selected)) {
						upgrade_type_idx = n;
						c.upgrade_type = upgrade_type_items[n].value;
					}

					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
	}

	if (s.opt_bingen_check_show && ImGui::CollapsingHeader("Check", s.opt_bingen_check_expand ? ImGuiTreeNodeFlags_DefaultOpen : 0))
	{
		ImGui::Text("Check Type");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		if (ImGui::BeginCombo("##CHECK_TYPE", check_type_value, 0))
		{
			for (int n = 0; n < IM_ARRAYSIZE(check_type_items); n++)
			{
				const bool is_selected = (check_type_idx == n);
				if (ImGui::Selectable(check_type_items[n].name, is_selected)) {
					check_type_idx = n;
					c.check_type = check_type_items[n].value;

					CalcCRC();
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::Text("[%08X]", c.out_data_crc);
		ImGui::SameLine();
		utils::HelpMarker("Used for verifying the original IAP bin.");
	}

	if (s.opt_bingen_encryption_show && ImGui::CollapsingHeader("Encryption", s.opt_bingen_encryption_expand ? ImGuiTreeNodeFlags_DefaultOpen : 0))
	{
		ImGui::Text("Encryption enable");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::Checkbox("##ENCRYPTION_ENABLE", &c.encryption_enable);
		ImGui::SameLine();
		utils::HelpMarker("Encrypt IAP bin data, programmers know the key, old programs need to know how to decrypt it.");


		if (c.encryption_enable)
		{
			ImGui::Text("Encryption Type");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::SetNextItemWidth(wv);
			if (ImGui::BeginCombo("##ENCRYPTION_TYPE", encryption_type_value, 0))
			{
				for (int n = 0; n < IM_ARRAYSIZE(encryption_type_items); n++)
				{
					const bool is_selected = (encryption_type_idx == n);
					if (ImGui::Selectable(encryption_type_items[n].name, is_selected)) {
						encryption_type_idx = n;
						c.encryption_type = encryption_type_items[n].value;
					}
					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)
			{
				c.encryption_keylen = 16;

				ImGui::Text("XOR Vector");
				ImGui::SameLine();
				ImGuiDCXAxisAlign(cl);
				ImGui::SetNextItemWidth(wv);
				ImGui::InputTextEx("##SOR Vector", "", c.encryption_xor, sizeof(c.encryption_xor), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
				ImGui::SameLine();
				ImGui::Text("hex");
				utils::ValidateHex16ByteText(c.encryption_xor, c.enc_xor);
			}
			else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)
			{
				c.encryption_keylen = 16;

				ImGui::Text("Encryption Key");
				ImGui::SameLine();
				ImGuiDCXAxisAlign(cl);
				ImGui::SetNextItemWidth(wv);
				ImGui::InputTextEx("##ENCRYPTION_KEY", "", c.encryption_key, sizeof(c.encryption_key), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
				ImGui::SameLine();
				ImGui::Text("hex");
				utils::ValidateHex16ByteText(c.encryption_key, c.enc_key);

				ImGui::Text("Encryption iv");
				ImGui::SameLine();
				ImGuiDCXAxisAlign(cl);
				ImGui::SetNextItemWidth(wv);
				ImGui::InputTextEx("##ENCRYPTION_IV", "", c.encryption_iv, sizeof(c.encryption_iv), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
				ImGui::SameLine();
				ImGui::Text("hex");
				utils::ValidateHex16ByteText(c.encryption_iv, c.enc_iv);
			}
		}
	}

	if (s.opt_bingen_configexe_show && ImGui::CollapsingHeader("Config exe", s.opt_bingen_configexe_expand ? ImGuiTreeNodeFlags_DefaultOpen : 0))
	{
		ImGui::Text("Configure related options during the upgrade process of the generated tool named \"iap.exe\".");

		ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		
		if (s.opt_bingen_exe_tree_node_vid_pid_show && ImGui::TreeNodeEx("VID/PID##CONFIG_EXE_VID_PID", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginTable("vprid", 4, flags))
			{
				ImGui::TableSetupColumn("Program", ImGuiTableColumnFlags_WidthFixed, 178.0f);
				ImGui::TableSetupColumn("VID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Report ID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < exe_config.vpridsNum; ++i) 
				{
					ImGui::PushID(i);
					auto& entry = exe_config.vprids[i];
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Checkbox("##ENTRY_CHECK", &entry.checked);
					ImGui::TableNextColumn();
					if (!entry.checked) ImGui::BeginDisabled();
					ImGui::InputTextEx("##BOOT_VID", NULL, entry.tvid, sizeof(entry.tvid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
					ImGui::TableNextColumn();
					ImGui::InputTextEx("##BOOT_PID", NULL, entry.tpid, sizeof(entry.tpid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
					ImGui::TableNextColumn();
					ImGui::InputTextEx("##BOOT_RID", NULL, entry.trid, sizeof(entry.trid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
					if (!entry.checked) ImGui::EndDisabled();
					utils::ValidateU16Text(entry.tvid, entry.vid);
					utils::ValidateU16Text(entry.tpid, entry.pid);
					utils::ValidateU8Text (entry.trid, entry.rid);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			
			ImGui::TreePop();
		}

		if (s.opt_bingen_exe_tree_node_options_show && ImGui::TreeNodeEx("Options##CONFIG_EXE_OPTIONS", 0 /*ImGuiTreeNodeFlags_DefaultOpen*/))
		{
			float al = 100.0f;
			float lo = 13.0f;

			if (ImGui::BeginTable("exeoption", 2, flags))
			{
				ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed, 178.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Retry count");
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(al);
				ImGui::InputScalar("##RETYR_NUM", ImGuiDataType_S32, &exe_config.retryNum, NULL, NULL, "%u");
				ImGui::SameLine();
				utils::HelpMarker("When the read ack times out, the command will be resent. The retry count is the maximum number of resends, Range 1~3.");
				if (exe_config.retryNum < 1)
					exe_config.retryNum = 1;
				if (exe_config.retryNum > 3)
					exe_config.retryNum = 3;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Reboot delay");
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(al);
				ImGui::InputScalar("##REBOOT_DELAY", ImGuiDataType_S32, &exe_config.rebootDelay, NULL, NULL, "%u");
				ImGui::SameLine();
				utils::HelpMarker("The delay time used for rebooting, in milliseconds, Range 0~1000.");
				if (exe_config.rebootDelay < IAP_REBOOT_DELAY_MIN)
					exe_config.rebootDelay = IAP_REBOOT_DELAY_MIN;
				if (exe_config.rebootDelay > IAP_REBOOT_DELAY_MAX)
					exe_config.rebootDelay = IAP_REBOOT_DELAY_MAX;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Switch delay");
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(al);
				ImGui::InputScalar("##SWITCH_DELAY", ImGuiDataType_S32, &exe_config.switchDelay, NULL, NULL, "%u");
				ImGui::SameLine();
				utils::HelpMarker("The delay time used for switching, in milliseconds, Range 0~1000.");
				if (exe_config.switchDelay < IAP_SWITCH_DELAY_MIN)
					exe_config.switchDelay = IAP_SWITCH_DELAY_MIN;
				if (exe_config.switchDelay > IAP_SWITCH_DELAY_MAX)
					exe_config.switchDelay = IAP_SWITCH_DELAY_MAX;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Search device timeout");
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(al);
				ImGui::InputScalar("##SEARCH_DEVICE_TIMEOUT", ImGuiDataType_S32, &exe_config.searchDeviceTimeout, NULL, NULL, "%u");
				ImGui::SameLine();
				utils::HelpMarker("Timeout for upgrade tools to find devices. \nAt least 1000. \nRecommended setting to a value greater than 'swiching/rebooting delay'.");
				if (exe_config.searchDeviceTimeout < 1000)
					exe_config.searchDeviceTimeout = 1000;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Read ACK timeout");
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(al);
				ImGui::InputScalar("##READ_ACK_TIMEOUT", ImGuiDataType_S32, &exe_config.readAckTimeout, NULL, NULL, "%u");
				ImGui::SameLine();
				utils::HelpMarker("Timeout for reading ack. \nAt least 1000.");
				if (exe_config.readAckTimeout < 1000)
					exe_config.readAckTimeout = 1000;

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}

		if (s.opt_bingen_exe_tree_node_icon_show && ImGui::TreeNodeEx("Icon##CONFIG_EXE_ICON", ImGuiTreeNodeFlags_DefaultOpen))
		{

			char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
			strcpy(bin, exe_config.icon.name);
			//=================================================================================
			// Input File Path
			//=================================================================================
			ImGui::InputTextEx("##icon.name", "", exe_config.icon.name, sizeof(exe_config.icon.name), ImVec2(0.0f, 0.0f), 0);
			//=================================================================================
			// File Select
			//=================================================================================
			ImGui::SameLine();
			if (ImGui::Button("Open##ICON_OPEN_BTN"))
				ifd::FileDialog::Instance().Open("ICON_OPEN", "Choose bin", "image {.png,.bmp}", false);
			//=================================================================================
			// File Dailog
			//=================================================================================
			if (ifd::FileDialog::Instance().IsDone("ICON_OPEN")) {
				if (ifd::FileDialog::Instance().HasResult()) {
					const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
					if (res.size() > 0) {
						std::filesystem::path path = res[0];
						std::string pathStr = path.u8string();
						strcpy(exe_config.icon.name, pathStr.c_str());
					}
				}
				ifd::FileDialog::Instance().Close();
			}
			//=================================================================================
			// Tip
			//=================================================================================
			if (exe_config.icon.name[0] != '\0')
			{
				ImGui::SameLine();
				if (exe_config.icon.size == 0) {
					ImGui::Text("Error: file not exists");
				}
				else {
					ImGui::Text("OK. file size = %dB", exe_config.icon.size);
				}
			}
			strcpy(exe_config.icon.name_gbk, utils::utf8_to_gbk(exe_config.icon.name).c_str());
			//=================================================================================
			// Get File Size
			//=================================================================================
			auto path = exe_config.icon.name_gbk;
			exe_config.icon.size = std::filesystem::is_regular_file(path) ?
				static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
			//=================================================================================
			// Reload File Binary Data | aligned to 16
			//=================================================================================
			if (exe_config.icon.size != 0 && (exe_config.icon.buffer.size() == 0 || strcmp(bin, exe_config.icon.name) != 0)) {
				exe_config.icon.buffer.resize(exe_config.icon.size);
				utils::readFileData(exe_config.icon.name_gbk, exe_config.icon.buffer.data());
				ResourceLoadTexture(exe_config.icon);
			}

			//=================================================================================
			// Preview
			//=================================================================================
			if (exe_config.icon.texID)
			{
				ImGui::Image(exe_config.icon.texID, exe_config.icon.texSize);
			}
			ImGui::TreePop();
		}

		if (s.opt_bingen_exe_tree_node_resource_show && ImGui::TreeNodeEx("Resource##CONFIG_EXE_RESOURCE", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginTable("resource_choose_table", 3, flags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 178.0f);
				ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("File");
				ImGui::TableHeadersRow();

				for (int i = 0; i < exe_config.rsrcsNum; ++i)
				{
					auto& rsrc = exe_config.rsrcs[i];

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text(GetResourceKeyName(rsrc.key).c_str());
					ImGui::TableNextColumn();
					ImGui::Button("preview");
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
					{
						ImGui::BeginTooltip();
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						ImVec2 pos = window->DC.CursorPos;
						window->DrawList->AddRectFilled(pos, pos + rsrc.texSize, ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)));
						ImGui::Image(rsrc.texID, rsrc.texSize);
						ImGui::EndTooltip();
					}
					ImGui::TableNextColumn();
					if (FileInput(i, rsrc.name, rsrc.name_gbk, BIN_NAME_BUFF_MAX_SIZE, rsrc.size, rsrc.buffer)) 
					{
						ResourceLoadTexture(rsrc);
					}
				}
				ImGui::EndTable();
			}
			ImGui::TreePop();
		}

		if (s.opt_bingen_exe_tree_node_style_show && ImGui::TreeNodeEx("Color##CONFIG_EXE_COLOR", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& style = ImGui::GetStyle();
			auto lambda = [&style](color_ui_t& item) {
				if (item.colID == ImGuiCol_COUNT)
					return;

				const char* name = ImGui::GetStyleColorName(item.colID);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(name);

				ImGui::TableNextColumn();
				ImGui::PushID(item.colID);
				ImGui::ColorEdit4("##color", (float*)&item.colVE, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex);
				if (memcmp(&item.colVE, &item.colVO, sizeof(ImVec4)) != 0)
				{
					ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Save")) { item.colVO = item.colVE; }
					ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Revert")) { item.colVE = item.colVO; }
				}
				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				ImGui::PopID();
			};

			if (ImGui::BeginTable("Color", 2, flags))
			{
				ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 178.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 240.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < exe_config.colorsNum; ++i)
				{
					lambda(exe_config.colors[i]);
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	
		ImGui::PopStyleVar();
	}

	if (s.opt_bingen_choosebin_show && ImGui::CollapsingHeader("Choose Bin", s.opt_bingen_choosebin_expand ? ImGuiTreeNodeFlags_DefaultOpen : 0))
	{
		ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		float al = 100.0f;
		float lo = 13.0f;

		if (ImGui::BeginTable("bin_choose_table", 3, flags))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 178.0f);
			ImGui::TableSetupColumn("Load Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("File");
			ImGui::TableHeadersRow();

			// Platform Bin
			if (c.upgrade_type == upgrade_type_e::UPGRADE_TYPE_PLATFORM_APP) {
				char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
				strcpy(bin, c.platform.name);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Platform");
				ImGui::TableNextColumn();
				ImGui::InputTextEx("h##PLATFORM_LOAD_ADDR_INPUT", NULL, c.platform.load_address, sizeof(c.platform.load_address), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
				utils::ValidateU32Text(c.platform.load_address, c.platform.load_addr);
				ImGui::TableNextColumn();
				//=================================================================================
				// Input File Path
				//=================================================================================
				ImGui::InputTextEx("##PLATFORM_BIN_FILE_NAME", "", c.platform.name, sizeof(c.platform.name), ImVec2(300, 0.0f), 0);
				//=================================================================================
				// Bin Select
				//=================================================================================
				ImGui::SameLine();
				if (ImGui::Button("Open##PLATFORM_BIN_OPEN_BTN"))
					ifd::FileDialog::Instance().Open("PLATFORM_BIN_OPEN", "Choose bin", "bin {.bin},.*", false);
				//=================================================================================
				// File Dailog
				//=================================================================================
				if (ifd::FileDialog::Instance().IsDone("PLATFORM_BIN_OPEN")) {
					if (ifd::FileDialog::Instance().HasResult()) {
						const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
						if (res.size() > 0) {
							std::filesystem::path path = res[0];
							std::string pathStr = path.u8string();
							strcpy(c.platform.name, pathStr.c_str());
						}
					}
					ifd::FileDialog::Instance().Close();
				}
				//=================================================================================
				// Tip
				//=================================================================================
				if (c.platform.name[0] != '\0')
				{
					ImGui::SameLine();
					if (c.platform.size == 0) {
						ImGui::Text("Error: file not exists");
					}
					else {
						ImGui::Text("OK. file size = %dB (padding:%dB)", c.platform.size, c.platform.data.size());
					}
				}
				strcpy(c.platform.name_gbk, utils::utf8_to_gbk(c.platform.name).c_str());
				//=================================================================================
				// Get File Size
				//=================================================================================
				auto path = c.platform.name_gbk;
				c.platform.size = std::filesystem::is_regular_file(path) ?
					static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
				//=================================================================================
				// Reload File Binary Data | aligned to 16
				//=================================================================================
				if (c.platform.size != 0 && (c.platform.data.size() == 0 || strcmp(bin, c.platform.name) != 0)) {
					ReloadPlatform();
				}
			}

			// App Bin
			{
				char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
				strcpy(bin, c.app.name);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("App");
				ImGui::TableNextColumn();
				ImGui::InputTextEx("h##APP_LOAD_ADDR_INPUT", NULL, c.app.load_address, sizeof(c.app.load_address), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
				utils::ValidateU32Text(c.app.load_address, c.app.load_addr);
				ImGui::TableNextColumn();
				//=================================================================================
				// Input File Path
				//=================================================================================
				ImGui::InputTextEx("##APP_BIN_FILE_NAME", "", c.app.name, sizeof(c.app.name), ImVec2(300, 0.0f), 0);
				//=================================================================================
				// Bin Select
				//=================================================================================
				ImGui::SameLine();
				if (ImGui::Button("Open##APP_BIN_OPEN_BTN"))
					ifd::FileDialog::Instance().Open("APP_BIN_OPEN", "Choose bin", "bin {.bin},.*", false);
				//=================================================================================
				// File Dailog
				//=================================================================================
				if (ifd::FileDialog::Instance().IsDone("APP_BIN_OPEN")) {
					if (ifd::FileDialog::Instance().HasResult()) {
						const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
						if (res.size() > 0) {
							std::filesystem::path path = res[0];
							std::string pathStr = path.u8string();
							strcpy(c.app.name, pathStr.c_str());
						}
					}
					ifd::FileDialog::Instance().Close();
				}
				//=================================================================================
				// Tip
				//=================================================================================
				if (c.app.name[0] != '\0')
				{
					ImGui::SameLine();
					if (c.app.size == 0) {
						ImGui::Text("Error: file not exists");
					}
					else {
						ImGui::Text("OK. file size = %dB (padding:%dB)", c.app.size, c.app.data.size());
					}
				}
				strcpy(c.app.name_gbk, utils::utf8_to_gbk(c.app.name).c_str());
				//=================================================================================
				// Get File Size
				//=================================================================================
				auto path = c.app.name_gbk;
				c.app.size = std::filesystem::is_regular_file(path) ?
					static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;
				//=================================================================================
				// Reload File Binary Data | aligned to 16
				//=================================================================================
				if (c.app.size != 0 && (c.app.data.size() == 0 || strcmp(bin, c.app.name) != 0)) {
					ReloadApp();
				}
			}

			ImGui::EndTable();
		}
	}

	ImGui::Separator();

	ImGui::Text("Export Directory");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::Text(utils::gbk_to_utf8(export_dir.generic_string()).c_str());


	if (!IsMergeable())
		ImGui::BeginDisabled();
	if (ImGui::Button("Export bin"))
	{
		if (!std::filesystem::exists(export_dir))
			export_dir = DefaultExportDir();

		if (GenerateBin())
		{
			Alert("Export bin finished.");
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Export to specified directory"))
	{
		std::cout << export_dir.generic_string() << std::endl;
		ifd::FileDialog::Instance().Open("ExportBinFile", "Choose export directory", "", false, utils::gbk_to_utf8(export_dir.generic_string()));
	}
	if (!IsMergeable())
		ImGui::EndDisabled();

	if (!std::filesystem::exists(export_path))
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Analysis bin"))
	{
		AnalysisBin();
		Alert("Analysis bin finished.");
	}
	if (!std::filesystem::exists(export_path))
		ImGui::EndDisabled();

	if (ifd::FileDialog::Instance().IsDone("ExportBinFile")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			std::string res = utils::utf8_to_gbk(ifd::FileDialog::Instance().GetResult().u8string());
			export_dir = std::filesystem::path(res);

			GenerateBin();

			Alert("Analysis bin finished.");
		}
		ifd::FileDialog::Instance().Close();
	}

	utils::AlertEx(&alert_flag, "Message", alert_message);
	
	ImGui::End();
}

void LoadBinGenINI()
{
	Loadini(ini_path);
	LoadEXEConfig(exe_config, ini_path_exe);

	auto& rsrc_enums = get_rsrc_enums();
	for (uint32_t i = 0; i < rsrc_enums.size(); ++i) {
		ResourceLoadDataAndTexture(exe_config.rsrcs[i]);
	}
	ResourceLoadDataAndTexture(exe_config.icon);
}

void SaveBinGenINI()
{
	static long frame_cnt = 0;
	if (frame_cnt % 100 == 0)
	{
		Saveini(ini_path);
		SaveEXEConfig(exe_config, ini_path_exe);
	}
	frame_cnt++;
}