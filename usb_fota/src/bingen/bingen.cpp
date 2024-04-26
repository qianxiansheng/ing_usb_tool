#include "bingen.h"

#include "bin.h"
#include "execonfig.h"

#include <sstream>
#include <fstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "util/utils.h"

#include "imgui/extensions/ImConsole.h"
#include "imgui/extensions/ImFileDialog.h"

#include "./bingen/template.cpp"

extern ImLogger* logger;
extern bin_config_t bin_config;
extern exe_config_t exe_config;
extern uint8_t* p_mem;
extern size_t s_mem;


static std::filesystem::path work_dir     = std::filesystem::current_path();
static std::filesystem::path ini_path     = std::filesystem::current_path().concat("\\bin_config.ini");
static std::filesystem::path ini_path_exe = std::filesystem::current_path().concat("\\exe_config.ini");
static std::filesystem::path export_path;

std::filesystem::path export_dir;

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
		uint32_t out_data_size = app_relative_address + c.app.data.size();
		c.out_data.resize(out_data_size);
		memset(c.out_data.data(), 0xFF, c.out_data.size());
		memcpy(c.out_data.data(), c.platform.data.data(), c.platform.data.size());
		memcpy(c.out_data.data() + app_relative_address, c.app.data.data(), c.app.data.size());
		c.out_data_crc = utils::crc16_modbus(c.out_data.data(), c.out_data.size());
	}
	break;
	case upgrade_type_e::UPGRADE_TYPE_APP_ONLY:
	default:
	{
		c.out_data_load_addr = c.app.load_addr;
		c.out_data.resize(c.app.data.size());
		memcpy(c.out_data.data(), c.app.data.data(), c.app.data.size());
		c.out_data_crc = utils::crc16_modbus(c.out_data.data(), c.out_data.size());
	}
	break;
	}
}

static void ReloadPlatform()
{
	int size = ((bin_config.platform.size + 15) & ~(15));
	bin_config.platform.data.resize(size);
	memset(bin_config.platform.data.data(), 0xFF, size);
	utils::readFileData(bin_config.platform.name_gbk, bin_config.platform.data.data());
}

static void ReloadApp()
{
	int size = ((bin_config.app.size + 15) & ~(15));
	bin_config.app.data.resize(size);
	memset(bin_config.app.data.data(), 0xFF, size);
	utils::readFileData(bin_config.app.name_gbk, bin_config.app.data.data());
}

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

static bool GenerateBin()
{
	if (!std::filesystem::exists(export_dir))
		std::filesystem::create_directory(export_dir);

	export_path = std::filesystem::path(export_dir).concat(ExportFileName());
	
	//=================================================================================
	// Reload File Binary Data
	//=================================================================================
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
		c.block_num = (c.out_data.size() - 1) / c.block_size + 1;
	}



	Exportbin(export_path);

	std::cout << export_path << std::endl;

	uint64_t size = std::filesystem::file_size(export_path);
	bin_config.out_data.resize(size);
	utils::readFileData(export_path, bin_config.out_data.data());

	p_mem = bin_config.out_data.data();
	s_mem = bin_config.out_data.size();

	logger->AddLog("[Export] %s\n", utils::gbk_to_utf8(export_path.generic_string()).c_str());


	// Generate exe
	std::vector<uint8_t> exeOut;

	uint32_t suffix = IAP_GEN_EXE_SUFFIX;

	uint32_t N = 0;
	uint32_t N_ = (uint32_t)bin_config.out_data.size();

	const uint8_t* exe_in_data = template_data;
	uint32_t k = sizeof(template_data);
	if (exe_in_data[k - 4] == (IAP_GEN_EXE_SUFFIX & 0xFF) &&
		exe_in_data[k - 3] == ((IAP_GEN_EXE_SUFFIX >> 8) & 0xFF) &&
		exe_in_data[k - 2] == ((IAP_GEN_EXE_SUFFIX >> 16) & 0xFF) &&
		exe_in_data[k - 1] == ((IAP_GEN_EXE_SUFFIX >> 24) & 0xFF))
	{
		logger->AddLog("[Gen exe] is already attached the iap bin data\n");
		N = exe_in_data[k - 8] | (exe_in_data[k - 7] << 8) | (exe_in_data[k - 6] << 16) | (exe_in_data[k - 5] << 24);
	}
	else
	{
		N = k;
	}
	exeOut.resize(N + N_ + 38);
	int i = 0;
	memcpy(exeOut.data() + i, exe_in_data, N);							i += N;
	memcpy(exeOut.data() + i, bin_config.out_data.data(), N_);			i += N_;
	append_16_little(exe_config.boot_vid,            exeOut.data() + i);i += 2;
	append_16_little(exe_config.boot_pid,            exeOut.data() + i);i += 2;
	append_8_little (exe_config.boot_rid,            exeOut.data() + i);i += 1;
	append_16_little(exe_config.app_vid,             exeOut.data() + i);i += 2;
	append_16_little(exe_config.app_pid,             exeOut.data() + i);i += 2;
	append_8_little (exe_config.app_rid,             exeOut.data() + i);i += 1;
	append_32_little(exe_config.retryNum,            exeOut.data() + i);i += 4;
	append_32_little(exe_config.rebootDelay,         exeOut.data() + i);i += 4;
	append_32_little(exe_config.switchDelay,         exeOut.data() + i);i += 4;
	append_32_little(exe_config.searchDeviceTimeout, exeOut.data() + i);i += 4;
	append_32_little(exe_config.readAckTimeout,      exeOut.data() + i);i += 4;
	append_32_little(N, exeOut.data() + i);								i += 4;
	append_32_little(suffix, exeOut.data() + i);						i += 4;

	assert(exeOut.size() == i);

	auto exepath = std::filesystem::path(export_dir).concat("iap.exe");
	std::ofstream ofs;
	ofs.open(exepath, std::ios::out | std::ios::binary);
	if (!ofs) {
		logger->AddLog("[Info] Failed to gen exe(File In Use) %s\n", utils::gbk_to_utf8(exepath.generic_string()).c_str());
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

void ShowBinGenWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = 0;
	if (!ImGui::Begin("Bin Editor", p_open, window_flags)) {
		ImGui::End();
		return;
	}

	bool show_alert_export_success = false;
	bool show_alert_file_in_use = false;
	bool show_alert_analysis_success = false;

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

	if (ImGui::CollapsingHeader("Base", ImGuiTreeNodeFlags_DefaultOpen))
	{

		ImGui::Text("Chip code");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		ImGui::InputTextEx("##CHIP_CODE", "", c.chip_code, sizeof(c.chip_code), ImVec2(300.0f, 0.0f), 0);
		ImGui::Text("Project code");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		ImGui::InputTextEx("##PROJ_CODE", "", c.proj_code, sizeof(c.proj_code), ImVec2(300.0f, 0.0f), 0);
		strcpy(buf, c.hard_version);
		ImGui::Text("Hardware version");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		ImGui::InputTextEx("##HARD_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
		if (utils::ValidateVersion(buf))	strcpy(c.hard_version, buf);
		strcpy(buf, c.soft_version);
		ImGui::SameLine();
		utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");
		ImGui::Text("Software version");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		ImGui::InputTextEx("##SOFT_VER", "", buf, sizeof(buf), ImVec2(300.0f, 0.0f), 0);
		if (utils::ValidateVersion(buf))	strcpy(c.soft_version, buf);
		ImGui::SameLine();
		utils::HelpMarker("The required format is 'Vx.y.z', such as 'V2.1.3'. Please note that 'V2.1.13' is not supported.");

		ImGui::Text("Block Size");
		ImGui::SameLine();
		ImGuiDCXAxisAlign(cl);
		ImGui::SetNextItemWidth(wv);
		ImGui::InputScalar("##BLOCK_SIZE", ImGuiDataType_U16, &c.block_size, NULL, "%u");
		if (c.block_size < 12) c.block_size = 12;
		if (c.block_size > 8192) c.block_size = 8192;
		if (c.out_data.size() == 0) c.block_num = 0;
		else c.block_num = (c.out_data.size() - 1) / c.block_size + 1;
		ImGui::SameLine();
		utils::HelpMarker("Range: between 12 and 8192 bytes.");

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

	if (ImGui::CollapsingHeader("Check", ImGuiTreeNodeFlags_DefaultOpen))
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

	if (ImGui::CollapsingHeader("Encryption"))
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

	if (ImGui::CollapsingHeader("Config exe"))
	{
		ImGui::Text("Configure related options during the upgrade process of the generated tool named \"iap.exe\".");

		ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		if (ImGui::BeginTable("vprid", 4, flags))
		{
			ImGui::TableSetupColumn("Program", ImGuiTableColumnFlags_WidthFixed, 178.0f);
			ImGui::TableSetupColumn("VID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Report ID", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("BOOT");
			ImGui::TableNextColumn();
			ImGui::InputTextEx("##BOOT_VID", NULL, exe_config.tbvid, sizeof(exe_config.tbvid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::TableNextColumn();
			ImGui::InputTextEx("##BOOT_PID", NULL, exe_config.tbpid, sizeof(exe_config.tbpid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::TableNextColumn();
			ImGui::InputTextEx("##BOOT_RID", NULL, exe_config.tbrid, sizeof(exe_config.tbrid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

			//ImGui::TableNextRow();
			//ImGui::TableNextColumn();
			//ImGui::Text("APP");
			//ImGui::TableNextColumn();
			//ImGui::InputTextEx("##APP_VID", NULL, exe_config.tavid, sizeof(exe_config.tavid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			//ImGui::TableNextColumn();
			//ImGui::InputTextEx("##APP_PID", NULL, exe_config.tapid, sizeof(exe_config.tapid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			//ImGui::TableNextColumn();
			//ImGui::InputTextEx("##APP_RID", NULL, exe_config.tarid, sizeof(exe_config.tarid), ImVec2(100.0f, 0.0f), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

			utils::ValidateU16Text(exe_config.tbvid, exe_config.boot_vid);
			utils::ValidateU16Text(exe_config.tbpid, exe_config.boot_pid);
			utils::ValidateU8Text(exe_config.tbrid, exe_config.boot_rid);

			utils::ValidateU16Text(exe_config.tavid, exe_config.app_vid);
			utils::ValidateU16Text(exe_config.tapid, exe_config.app_pid);
			utils::ValidateU8Text(exe_config.tarid, exe_config.app_rid);

			ImGui::EndTable();
		}

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
	}

	if (ImGui::CollapsingHeader("Choose Bin", ImGuiTreeNodeFlags_DefaultOpen))
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
			show_alert_export_success = true;
		}
		else 
		{
			show_alert_file_in_use = true;
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
	utils::Alert(show_alert_file_in_use, "Message##m3", "Export failed because the file is in use.");
	


	ImGui::End();
}

void LoadBinGenINI()
{
	Loadini(ini_path);
	LoadEXEConfig(ini_path_exe);
}

void SaveBinGenINI()
{
	static long frame_cnt = 0;
	if (frame_cnt % 100 == 0)
	{
		Saveini(ini_path);
		SaveEXEConfig(ini_path_exe);
	}
	frame_cnt++;
}