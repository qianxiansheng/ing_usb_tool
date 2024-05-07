#include "pch.h"

#include <iostream>
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <sstream>

#include "winbase.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "libusb-1.0/libusb.h"

#include "util/thread_pool.h"
#include "util/utils.h"
#include "usb/usb_utils.h"

#include "imgui/extensions/ImConsole.h"

#include "passthrough/usbdevtree.h"
#include "passthrough/passthrough.h"
#include "bingen/bingen.h"
#include "bingen/memedit.h"
#include "ingiap/ingiap.h"

#include "imgui/extensions/ImFileDialog.h"

#include "setting.h"


#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;
extern app_settings_t gSettings;

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

ImLogger* logger;
ThreadPool* pool;

extern std::filesystem::path export_dir;

extern void* CreateTexture(uint8_t* data, int w, int h, char fmt);
extern void DeleteTexture(void* tex);

static void Cube_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
	std::cout << "ClearAll" << std::endl;
}

static void* Cube_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
	return &export_dir;
}

static void Cube_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	std::string s(line);

	size_t i = s.find_first_of('=');
	if (i >= s.size()) return;

	auto name = s.substr(0, i);
	auto value = s.substr(i + 1);


	if (name == "export_dir")
	{
		export_dir = value;
	}
}

static void Cube_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
	buf->append("[IAP][config]\n");
	buf->appendf("export_dir=%s\n", export_dir.generic_string().c_str());
}

static void LoadSettings()
{
	LoadSetting(SETTINGS_INI);
}

static void SaveSettings() 
{
	static long frame_cnt = 0;
	if (frame_cnt % 100 == 0)
	{
		SaveSetting(SETTINGS_INI);
	}
	frame_cnt++;
}

static bool main_init(int argc, char* argv[])
{
	// Enable Dock
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Load setting
	LoadSettings();

	// Load ini
	LoadBinGenINI();

	// Theme
	ImGui::StyleColorsLight();

	// File dialog adapter
	ifd::FileDialog::Instance().CreateTexture = [](uint8_t* data, int w, int h, char fmt) -> void* {
		return CreateTexture(data, w, h, fmt);
	};
	ifd::FileDialog::Instance().DeleteTexture = [](void* tex) {
		DeleteTexture(tex);
	};

	// Font
	io.Fonts->AddFontFromFileTTF(DEFAULT_FONT, 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	// ImConsole
	logger = new ImLogger();
	pool = new ThreadPool(25);

	// USB device tree
	InitUSBDeviceTree();

	// HID Tool
	//InitHIDTool();

	static ImGuiSettingsHandler ini_handler;
	ini_handler.TypeName = "IAP";
	ini_handler.TypeHash = ImHashStr("IAP");
	ini_handler.ClearAllFn = Cube_ClearAll;
	ini_handler.ReadOpenFn = Cube_ReadOpen;
	ini_handler.ReadLineFn = Cube_ReadLine;
	ini_handler.ApplyAllFn = NULL;
	ini_handler.WriteAllFn = Cube_WriteAll;
	ImGui::AddSettingsHandler(&ini_handler);

    return true;
}

static void main_shutdown(void)
{
	ReleaseIAPWindow();

	delete logger;
}

static void ShowLog(bool* p_open)
{
	logger->Draw("logger", p_open);
}

static void ShowRootWindowMenu()
{
	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			if (ImGui::MenuItem("Open ini")) {
			}

			if (ImGui::MenuItem("Save", "Ctrl + S")) {
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Option")) {

			app_settings_t& c = gSettings;

			ImGui::MenuItem("Demo Window", NULL, &c.opt_showdemowindow);
			ImGui::MenuItem("Log Window", NULL, &c.opt_showlogwindow);
			ImGui::MenuItem("Binary Viewer", NULL, &c.opt_showbinviewerwindow);
			ImGui::Separator();
			ImGui::MenuItem("USB Device tree", NULL, &c.opt_showusbdevicetreewindow);
			ImGui::MenuItem("Bin Editor", NULL, &c.opt_showbingenwindow);
			ImGui::MenuItem("USB Send", NULL, &c.opt_showusbsendwindow);
			ImGui::Separator();
			ImGui::MenuItem("IAP", NULL, &c.opt_showiapwindow);

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

static void ShowRootWindow(bool* p_open)
{
	app_settings_t& c = gSettings;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	if (c.opt_fullscreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("-", p_open, window_flags);

	if (c.opt_fullscreen) {
		ImGui::PopStyleVar();	// WindowBorderSize
		ImGui::PopStyleVar();	// WindowRounding
	}
	ImGui::PopStyleVar();	// WindowPadding

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

	ShowRootWindowMenu();

	ImGui::End();
	if (c.opt_showdemowindow) {
		ImGui::ShowDemoWindow(&c.opt_showdemowindow);
	}

	if (c.opt_showusbdevicetreewindow) {
		ShowUSBDeviceTreeWindow(&c.opt_showusbdevicetreewindow);
	}

	if (c.opt_showusbsendwindow) {
		ShowUSBSendWindow(&c.opt_showusbsendwindow);
	}

	if (c.opt_showbingenwindow) {
		ShowBinGenWindow(&c.opt_showbingenwindow);
	}

	if (c.opt_showlogwindow) {
		ShowLog(&c.opt_showlogwindow);
	}

	if (c.opt_showbinviewerwindow) {
		ShowBinViewer(&c.opt_showbinviewerwindow);
	}

	if (c.opt_showiapwindow) {
		ShowIAPWindow(&c.opt_showiapwindow);
	}

}

static int main_gui()
{
	ShowRootWindow(&show_root_window);

	SaveBinGenINI();
	SaveSettings();

    return 0;
}


#include "gui_glue_gl3.cpp"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int nShowCmd)
{
	return main_(__argc, __argv);
}

int main()
{
	return main_(__argc, __argv);
}