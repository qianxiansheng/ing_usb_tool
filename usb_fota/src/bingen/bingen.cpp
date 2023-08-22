#include "bingen.h"

#include "bin.h"

#include <sstream>
#include <fstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "util/utils.h"

#include "imgui/extensions/ImConsole.h"
#include "imgui/extensions/ImFileDialog.h"

extern ImLogger* logger;
extern bin_config_t bin_config;
extern uint8_t* p_mem;
extern size_t s_mem;


static std::filesystem::path work_dir = std::filesystem::current_path();
static std::filesystem::path ini_path = std::filesystem::current_path().concat("\\bin_config.ini");
static std::filesystem::path export_path;

std::filesystem::path export_dir;


static void CalcCrc()
{
	auto& c = bin_config;

	if (c.in_size == 0)
		return;

	if (c.check_type == CHECK_TYPE_CRC)
	{
		uint16_t crc16 = utils::crc16_modbus(c.in_data.data(), c.in_data.size());
		c.in_crc = crc16;
	}
	else if (c.check_type == CHECK_TYPE_SUM)
	{
		uint16_t sum = utils::sum_16(c.in_data.data(), c.in_data.size());
		c.in_crc = sum;
	}
}
static void ReloadInFile()
{
	int size = ((bin_config.in_size - 1) / 16 + 1) * 16;

	bin_config.in_data.resize(size);
	memset(bin_config.in_data.data(), 0xFF, size);
	utils::readFileData(bin_config.in_name_gbk, bin_config.in_data.data());

	CalcCrc();
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
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT)
		ss << "PB_";
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT_APP)
		ss << "PBA_";

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

static void GenerateBin()
{
	export_path = std::filesystem::path(export_dir).concat(ExportFileName());

	Exportbin(export_path);

	std::cout << export_path << std::endl;

	uint64_t size = std::filesystem::file_size(export_path);
	bin_config.out_data.resize(size);
	utils::readFileData(export_path, bin_config.out_data.data());

	p_mem = bin_config.out_data.data();
	s_mem = bin_config.out_data.size();

	logger->AddLog("[Export] %s\n", utils::gbk_to_utf8(export_path.generic_string()).c_str());
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

void ShowBinGenWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("Bin Editor", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	bool show_alert_export_success = false;
	bool show_alert_analysis_success = false;

	const int cl = 200;
	char bin[BIN_NAME_BUFF_MAX_SIZE] = { 0 };
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
		{"PLATFORM + BOOT", UPGRADE_TYPE_PLATFORM_BOOT},
		{"PLATFORM + BOOT + APP", UPGRADE_TYPE_PLATFORM_BOOT_APP}
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
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT)		upgrade_type_idx = 2;
	else if (c.upgrade_type == UPGRADE_TYPE_PLATFORM_BOOT_APP)	upgrade_type_idx = 3;
	else														upgrade_type_idx = 0;

	if (c.encryption_type == BIN_ENCRYPTION_TYPE_XOR)			encryption_type_idx = 0;
	else if (c.encryption_type == BIN_ENCRYPTION_TYPE_AES)		encryption_type_idx = 1;
	else														encryption_type_idx = 1;

	const char* check_type_value = check_type_items[check_type_idx].name;
	const char* upgrade_type_value = upgrade_type_items[upgrade_type_idx].name;
	const char* encryption_type_value = encryption_type_items[encryption_type_idx].name;

	ShowSubTitle("Base");//=============================================================

	ImGui::Text("Chip code");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##CHIP_CODE", "", c.chip_code, sizeof(c.chip_code), ImVec2(300.0f, 0.0f), 0);
	ImGui::Text("Project code");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##PROJ_CODE", "", c.proj_code, sizeof(c.proj_code), ImVec2(300.0f, 0.0f), 0);
	strcpy(buf, c.hard_version);
	ImGui::Text("Hardware version");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##HARD_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
	if (utils::ValidateVersion(buf))	strcpy(c.hard_version, buf);
	strcpy(buf, c.soft_version);
	ImGui::SameLine();
	utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");
	ImGui::Text("Software version");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##SOFT_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
	if (utils::ValidateVersion(buf))	strcpy(c.soft_version, buf);
	ImGui::SameLine();
	utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");

	ImGui::Text("Block Size");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputScalar("##BLOCK_SIZE", ImGuiDataType_U16, &c.block_size, NULL, "%u");
	if (c.block_size < 12) c.block_size = 12;
	if (c.block_size > 8192) c.block_size = 8192;
	if (c.in_size == 0) c.block_num = 0;
	else c.block_num = (c.in_size - 1) / c.block_size + 1;
	ImGui::SameLine();
	utils::HelpMarker("Range: between 12 and 8192 bytes.");

	ImGui::Text("Upgrate Type");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
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

	ShowSubTitle("Check");//============================================================

	ImGui::Text("Check Type");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	if (ImGui::BeginCombo("##CHECK_TYPE", check_type_value, 0))
	{
		for (int n = 0; n < IM_ARRAYSIZE(check_type_items); n++)
		{
			const bool is_selected = (check_type_idx == n);
			if (ImGui::Selectable(check_type_items[n].name, is_selected)) {
				check_type_idx = n;
				c.check_type = check_type_items[n].value;

				CalcCrc();
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Text("[%08X]", c.in_crc);
	ImGui::SameLine();
	utils::HelpMarker("Used for verifying the original IAP bin.");

	ShowSubTitle("Encryption");//=======================================================


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
			ImGui::InputTextEx("##ENCRYPTION_KEY", "", c.encryption_key, sizeof(c.encryption_key), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			ImGui::Text("hex");
			utils::ValidateHex16ByteText(c.encryption_key, c.enc_key);

			ImGui::Text("Encryption iv");
			ImGui::SameLine();
			ImGuiDCXAxisAlign(cl);
			ImGui::InputTextEx("##ENCRYPTION_IV", "", c.encryption_iv, sizeof(c.encryption_iv), ImVec2(300.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			ImGui::Text("hex");
			utils::ValidateHex16ByteText(c.encryption_iv, c.enc_iv);
		}
	}

	ShowSubTitle("Choose Bin");//======================================================

	strcpy(bin, c.in_name);
	//=================================================================================
	// Input File Path
	//=================================================================================
	ImGui::Text("Bin");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::InputTextEx("##BIN_FILE_NAME", "", c.in_name, sizeof(c.in_name), ImVec2(600, 0.0f), 0);
	ImGui::SameLine();


	//=================================================================================
	// Tip
	//=================================================================================
	if (c.in_name[0] != '\0')
	{
		ImGui::SameLine();
		if (c.in_size == 0) {
			ImGui::Text("Error: file not exists");
		}
		else {
			ImGui::Text("OK. file size = %dB (padding:%dB)", c.in_size, c.in_data.size());
		}
	}
	//=================================================================================
	// File Dailog
	//=================================================================================
	if (ifd::FileDialog::Instance().IsDone("BIN_OPEN")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			if (res.size() > 0) {
				std::filesystem::path path = res[0];
				std::string pathStr = path.u8string();
				strcpy(c.in_name, pathStr.c_str());
			}
		}
		ifd::FileDialog::Instance().Close();
	}

	strcpy(c.in_name_gbk, utils::utf8_to_gbk(c.in_name).c_str());
	//=================================================================================
	// Get File Size
	//=================================================================================
	auto path = c.in_name_gbk;
	c.in_size = std::filesystem::is_regular_file(path) ?
		static_cast<uint32_t>(std::filesystem::file_size(path)) : 0;

	//=================================================================================
	// Reload File Binary Data
	//=================================================================================
	if (c.in_size != 0 && (c.in_data.size() == 0 || strcmp(bin, c.in_name) != 0))
		ReloadInFile();

	ImGui::Text("Export Directory");
	ImGui::SameLine();
	ImGuiDCXAxisAlign(cl);
	ImGui::Text(utils::gbk_to_utf8(export_dir.generic_string()).c_str());

	//=================================================================================
	// Bin Select
	//=================================================================================
	if (ImGui::Button("Import Bin"))
		ifd::FileDialog::Instance().Open("BIN_OPEN", "Choose bin", "bin {.bin},.*", false);

	if (c.in_size == 0)
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Export bin"))
	{
		if (!std::filesystem::exists(export_dir))
			export_dir = DefaultExportDir();

		GenerateBin();
		show_alert_export_success = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Export to specified directory"))
	{
		std::cout << export_dir.generic_string() << std::endl;
		ifd::FileDialog::Instance().Open("ExportBinFile", "Choose export directory", "", false, utils::gbk_to_utf8(export_dir.generic_string()));
	}
	if (c.in_size == 0)
		ImGui::EndDisabled();

	if (!std::filesystem::exists(export_path))
		ImGui::BeginDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Analysis bin"))
	{
		AnalysisBin();
		show_alert_analysis_success = true;
	}
	if (!std::filesystem::exists(export_path))
		ImGui::EndDisabled();

	if (ifd::FileDialog::Instance().IsDone("ExportBinFile")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			std::string res = utils::utf8_to_gbk(ifd::FileDialog::Instance().GetResult().u8string());
			export_dir = std::filesystem::path(res);

			GenerateBin();

			show_alert_analysis_success = true;
		}
		ifd::FileDialog::Instance().Close();
	}

	utils::Alert(show_alert_export_success, "Message##m1", "Export bin finished.");
	utils::Alert(show_alert_analysis_success, "Message##m2", "Analysis bin finished.");


	ImGui::End();
}

void LoadBinGenINI()
{
	Loadini(ini_path);
}

void SaveBinGenINI()
{
	static long frame_cnt = 0;
	if (frame_cnt % 100 == 0)
		Saveini(ini_path);
	frame_cnt++;
}