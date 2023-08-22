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

#include "imgui/extensions/ImFileDialog.h"


#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;
static bool opt_fullscreen = true;
static bool opt_showdemowindow = false;
static bool opt_showlogwindow = true;
static bool opt_showusbdevicetreewindow = false;
static bool opt_showusbsendwindow = false;
static bool opt_showbingenwindow = true;
static bool opt_showbinviewerwindow = true;

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

ImLogger* logger;

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

static bool main_init(int argc, char* argv[])
{
	// Enable Dock
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

	// USB device tree
	InitUSBDeviceTree();

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

			ImGui::MenuItem("Demo Window", NULL, &opt_showdemowindow);
			ImGui::MenuItem("Log Window", NULL, &opt_showlogwindow);
			ImGui::MenuItem("Binary Viewer", NULL, &opt_showbinviewerwindow);
			ImGui::Separator();
			ImGui::MenuItem("USB Device tree", NULL, &opt_showusbdevicetreewindow);
			ImGui::MenuItem("Bin Editor", NULL, &opt_showbingenwindow);
			ImGui::MenuItem("USB Send", NULL, &opt_showusbsendwindow);

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

static void ShowRootWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	if (opt_fullscreen) {
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

	if (opt_fullscreen) {
		ImGui::PopStyleVar();	// WindowBorderSize
		ImGui::PopStyleVar();	// WindowRounding
	}
	ImGui::PopStyleVar();	// WindowPadding

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

	ShowRootWindowMenu();

	ImGui::End();
	if (opt_showdemowindow) {
		ImGui::ShowDemoWindow(&opt_showdemowindow);
	}

	if (opt_showusbdevicetreewindow) {
		ShowUSBDeviceTreeWindow(&opt_showusbdevicetreewindow);
	}

	if (opt_showusbsendwindow) {
		ShowUSBSendWindow(&opt_showusbsendwindow);
	}

	if (opt_showbingenwindow) {
		ShowBinGenWindow(&opt_showbingenwindow);
	}

	if (opt_showlogwindow) {
		ShowLog(&opt_showlogwindow);
	}

	if (opt_showbinviewerwindow) {
		ShowBinViewer(&opt_showbinviewerwindow);
	}
}

static int main_gui()
{
	ShowRootWindow(&show_root_window);

	SaveBinGenINI();

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