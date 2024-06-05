#include "pch.h"

#include <iostream>
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <sstream>
#include <future>
#include <thread>
#include <chrono>
#include <atomic>

#include "winbase.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"

#include "libusb-1.0/libusb.h"

#include "util/thread_pool.h"
#include "util/utils.h"
#include "util/myqueue.h"
#include "util/PEUtils.h"

#include "iap_config.h"
#include "usb.h"
#include "iap.h"
#include "itexture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;
static bool opt_fullscreen = true;

static bool alert_flag = false;
static char alert_message[256] = { 0 };
static char label_message[256] = { 0 };
static bool g_auto_start = false;

static std::atomic_bool cancel_flag(false);
static bool g_file_valid_flag = false;

extern iap_config_t iap_config;
extern std::filesystem::path selfName;

enum {
	MAINSTATE_IDLE,
	MAINSTATE_GETDEVINFO,
	MAINSTATE_IAP_RUNNING,
	MANISTATE_MONITOR_DEV,
	MAINSTATE_AUTO_IAP_MODE_IDLE,
	MAINSTATE_AUTO_IAP_MODE_RUNNING,
	MAINSTATE_AUTO_IAP_MODE_MONITOR_DEV,
};
static std::atomic<int> g_main_state = MAINSTATE_IDLE;

uint32_t progress_limit = 100;
uint32_t progress_pos = 100;

char deviceVersion[5] = "0000";
char deviceVID[5] = "0000";
char devicePID[5] = "0000";

HIDDevice hid = { 0 };

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

ThreadPool* pool;

enum UIEventTypeEnum
{
	UI_EVENT_TYPE_ALERT_MESSAGE = 0x30,
	UI_EVENT_TYPE_LABEL_MESSAGE = 0x31,
	UI_EVENT_TYPE_UPDATE_DEVINFO = 0x32,
	UI_EVENT_TYPE_EXIT = 0xF0,
};

typedef struct
{
	uint32_t eventType;

	uint16_t vid;
	uint16_t pid;
	uint16_t releaseVersion;
	std::string message;
} UIEvent;

BlockingQueue<UIEvent>* queueUI;


static bool UIThreadEventHandler()
{
	UIEvent e;
	queueUI->take(e);
	printf("event type: %d %s\n", e.eventType, e.message.c_str());
	switch (e.eventType)
	{
	case UI_EVENT_TYPE_ALERT_MESSAGE:
		alert_flag = true;
		strcpy(alert_message, e.message.c_str());
		break;

	case UI_EVENT_TYPE_LABEL_MESSAGE:
		strcpy(label_message, e.message.c_str());
		break;

	case UI_EVENT_TYPE_UPDATE_DEVINFO:
		sprintf_s(deviceVID, 5, "%04X", e.vid);
		sprintf_s(devicePID, 5, "%04X", e.pid);
		sprintf_s(deviceVersion, 5, "%04X", e.releaseVersion);
		break;

	case UI_EVENT_TYPE_EXIT:
		return false;
	}

	return true;
}

static void UIThreadPutEvent(UIEventTypeEnum eventType)
{
	UIEvent e;
	e.eventType = eventType;
	queueUI->put(e);
}
static void UIThreadPutEvent(UIEvent&& e)
{
	queueUI->put(e);
}
static void UIThreadPutEvent(UIEvent& e)
{
	queueUI->put(e);
}

static void UIThreadPutEvent_AlertMessage(std::string message)
{
	UIEvent e;
	e.eventType = UI_EVENT_TYPE_ALERT_MESSAGE;
	e.message = message;
	queueUI->put(e);
}
static void UIThreadPutEvent_LabelMessage(std::string message)
{
	UIEvent e;
	e.eventType = UI_EVENT_TYPE_LABEL_MESSAGE;
	e.message = message;
	queueUI->put(e);
}
static void UIThreadPutEvent_UpdateDeviceInfo(uint16_t vid, uint16_t pid, uint16_t releaseVersion)
{
	UIEvent e;
	e.eventType = UI_EVENT_TYPE_UPDATE_DEVINFO;
	e.vid = vid;
	e.pid = pid;
	e.releaseVersion = releaseVersion;
	queueUI->put(e);
}
static void UIThreadPutEvent_Exit()
{
	UIEvent e;
	e.eventType = UI_EVENT_TYPE_EXIT;
	queueUI->put(e);
}

static void UIThread()
{
	while (UIThreadEventHandler())
	{
	}
}

static void SearchDevice(std::vector<std::tuple<uint16_t, uint16_t, uint8_t>> vpridList, 
	uint32_t milliseconds, HIDDevice* dev)
{
	auto overTime = std::chrono::system_clock::now() + 
		std::chrono::milliseconds(milliseconds);

	while (!cancel_flag && std::chrono::system_clock::now() < overTime)
	{
		if (OpenHIDInterface(vpridList, dev) == HID_FIND_SUCCESS)
		{
			printf("report ID:%02X\n", dev->reportId);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

static ImVec4 dark(const ImVec4 v, float n)
{
	return ImVec4(
		ImMax(0.0f, v.x - n),
		ImMax(0.0f, v.y - n),
		ImMax(0.0f, v.z - n),
		ImMax(0.0f, v.w - n)
	);
}

static void LoadStyle()
{
	auto& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = iap_config.color[0];
	style.Colors[ImGuiCol_WindowBg] = iap_config.color[1];
	style.Colors[ImGuiCol_Button] = iap_config.color[2];
	style.Colors[ImGuiCol_ButtonHovered] = iap_config.color[3];
	style.Colors[ImGuiCol_ButtonActive] = iap_config.color[4];
	style.Colors[ImGuiCol_PlotHistogram] = iap_config.color[5];
	style.Colors[ImGuiCol_FrameBg] = iap_config.color[6];
	style.Colors[ImGuiCol_PopupBg] = iap_config.color[1];
	style.Colors[ImGuiCol_TitleBgActive] = dark(iap_config.color[1], 0.1f);
}


extern IAPContext iap_ctx;
#define showLabel UIThreadPutEvent_LabelMessage
#define showAlert UIThreadPutEvent_AlertMessage
#define showAlert_(msg) \
	UIThreadPutEvent_LabelMessage(msg);\
	UIThreadPutEvent_AlertMessage(msg);
void onbusinessOk(IAPContext& ctx)
{
	progress_limit = ctx.bin_size_limit;
	progress_pos   = ctx.bin_size_pos;
}

static void IAPProcess()
{
	showLabel("Open Device...");

	SearchDevice(iap_config.vpidEntryTable, iap_config.searchDeviceTimeout, &hid);

	if (hid.phandle == NULL)
	{
		showLabel("Open Failed!");
		return;
	}
	else
	{
		showLabel("Open Success! BOOT");
	}

	uint8_t* header = iap_config.iap_bin.data();
	uint16_t block_size = header[66] + (header[67] << 8);

	InitIAPContext(hid, iap_config.iap_bin, block_size,
		iap_config.retryNum,
		iap_config.readAckTimeout,
		iap_config.switchDelay,
		onbusinessOk, &iap_ctx);
	try {
		iap_run(iap_ctx);
	}
	catch (std::exception& e) {
		showAlert_(e.what());
		CloseHIDInterface(hid);
		return;
	}
	if (iap_ctx.primary_status == IAP_STATUS_COMPLETE)
	{
		showAlert_("PASS");
	}
	else
	{
		if (iap_ctx.terinmateReason == IAP_TERMINATE_REASON_PROTOCOL_ERROR)
		{
			char buf[128] = { 0 };
			sprintf(buf, "Upgrade error, please try again.\nError code:0x%02X\n%s",
				iap_ctx.ackCode, iap_ack_str(iap_ctx.ackCode).c_str());
			showAlert_(buf);
		}
		else if (iap_ctx.terinmateReason == IAP_TERMINATE_REASON_OVER_THE_MAX_RETRY_COUNT)
		{
			showAlert_("Timeout error, please try again.");
		}
		else
		{
			showAlert_("Upgrade error, please try again.");
		}
	}
	CloseHIDInterface(hid);
}

static void DeviceInfoThread()
{
	if (g_main_state != MAINSTATE_IDLE)
		return;
	g_main_state = MAINSTATE_GETDEVINFO;

	showAlert_("...");
	try {
		uint16_t vid, pid, release;
		if (GetUSBDeviceInfo(iap_config.vpidEntryTable, &vid, &pid, &release)) {
			UIThreadPutEvent_UpdateDeviceInfo(vid, pid, release);
			showAlert_("OK");
		} else {
			showAlert_("FAILED");
		}
	} catch (std::exception& e) {
		showAlert_("FAILED");
	}

	g_main_state = MAINSTATE_IDLE;
}

static void IAPThread()
{
	if (g_main_state != MAINSTATE_IDLE)
		return;
	g_main_state = MAINSTATE_IAP_RUNNING;

	IAPProcess();

	g_main_state = MAINSTATE_IDLE;
}

static void AutoIAPThread()
{
	if (g_main_state != MAINSTATE_IDLE)
		return;
	g_main_state = MAINSTATE_AUTO_IAP_MODE_IDLE;

	while (g_auto_start) {
		g_main_state = MAINSTATE_AUTO_IAP_MODE_RUNNING;
		IAPProcess();
		g_main_state = MAINSTATE_AUTO_IAP_MODE_IDLE;
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	}

	g_main_state = MAINSTATE_IDLE;
}

static void DevMoniterThread()
{
	
}


bool main_init(int argc, char* argv[])
{
	ImGuiIO& io = ImGui::GetIO();
	// disable imgui.ini
	io.IniFilename = NULL;
	io.LogFilename = NULL;

	selfName = argv[0];
	// selfName = "D:\\myResource\\work\\c++\\ing_usb_fota\\ing_usb_fota\\usb_fota\\output\\iap.exe";

	// Theme
	ImGui::StyleColorsLight();

	// Font
	io.Fonts->AddFontFromFileTTF(DEFAULT_FONT, 36.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	// Thread pool
	pool = new ThreadPool(10);

	// UI thread
	queueUI = new BlockingQueue<UIEvent>(32);
	pool->enqueue(UIThread);

	// IAP bin data
	if (!LoadData()) {
		UIThreadPutEvent_LabelMessage("File corruption!\nPlease regenerate the tool.");
		UIThreadPutEvent_AlertMessage("File corruption!\nPlease regenerate the tool.");
	}
	else {
		g_file_valid_flag = true;
	}

	// Style
	LoadStyle();

	// Icon data
	LoadIconTexture();

	// HID
	HIDInit();

	return true;
}

extern bool stop;
void main_shutdown(void)
{
	UIThreadPutEvent_Exit();
	stop = true;
	cancel_flag = true;
	// delete pool;
	HIDExit();
}

#ifndef IMGUI_DEFINE_MATH_OPERATORS
static ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
	return ImVec2(a.x + b.x, a.y + b.y);
}
#endif

static void DrawImage(int key, float alpha) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;
	auto& [pos, size] = iap_config.gResourceLayoutMap[key];
	window->DrawList->AddImage(iap_config.gResourceMap[key]->_texID,
		p + pos,
		p + pos + size, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)));
}
static void DrawImage(int key) {
	DrawImage(key, 1.0f);
}

static bool RDRImageCheckbox(const char* label, int key, int key_checked, bool* v)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);

	auto& [pos, size] = iap_config.gResourceLayoutMap[key];

	const ImRect total_bb(p + pos, p + pos + size);
	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id))
	{
		return false;
	}

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
	if (pressed)
	{
		*v = !(*v);
		ImGui::MarkItemEdited(id);
	}
	const int col = *v ? key_checked : key;

	// Reset
	ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

	DrawImage(col, g.Style.Alpha);

	return pressed;
}

static bool RDRImageButton(const char* label, int key, int key_hover, int key_active)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiID id = window->GetID(label);

	ImVec2 p = window->DC.CursorPos;
	auto& [pos, size] = iap_config.gResourceLayoutMap[key];
	const ImRect bb(p + pos, p + pos + size);
	ImGui::ItemSize(size);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

	// Reset
	ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
	
	// Render
	const int col = (held && hovered) ? key_active : hovered ? key_hover : key;
	DrawImage(col, g.Style.Alpha);

	return pressed;
}

static void RDRProgress(float fraction, ImVec2 pos, ImVec2 size)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;

	ImVec2 rsize = ImVec2(size.x * fraction, size.y);
	window->DrawList->AddRectFilled(p + pos, p + pos + rsize, ImGui::GetColorU32(ImGuiCol_PlotHistogram));
	window->DrawList->AddRectFilled(p + pos, p + pos + size, ImGui::GetColorU32(ImGuiCol_FrameBg));
}

static void TextCentered(const char* text, ImVec2 lt, ImVec2 rb)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;
	std::vector<uint16_t> lnList;

	const char* pText = text;

	size_t text_index = 0;
	size_t text_length = strlen(text);

	size_t line_begin_index = text_index;

	float wrap_width = rb.x - lt.x;
	float line_text_width = 0.0f;

	auto h = ImGui::GetTextLineHeight();

	// Calc lnList(line list)
	while (text_index < text_length)
	{
		ImVec2 text_size = ImGui::CalcTextSize(pText + text_index, pText + text_index + 1);

		line_text_width += text_size.x;

		if (*(pText + text_index) == '\n')
		{
			auto lnDescriptor = (line_begin_index << 8) | text_index;
			lnList.push_back((uint16_t)lnDescriptor);

			text_index++;

			line_text_width = 0;
			line_begin_index = text_index;
		}

		if (line_text_width > wrap_width)
		{
			auto lnDescriptor = (line_begin_index << 8) | text_index;
			lnList.push_back((uint16_t)lnDescriptor);

			line_text_width = 0;
			line_begin_index = text_index;
		}
		text_index++;
	}
	auto lnDescriptor = (line_begin_index << 8) | text_index;
	lnList.push_back((uint16_t)lnDescriptor);

	ImVec2 cursor;

	// Draw line
	for (size_t i = 0; i < lnList.size(); ++i)
	{
		uint16_t& lnDescriptor = lnList[i];

		uint8_t line_begin_index = (lnDescriptor >> 8) & 0xFF;
		uint8_t line_end_index = lnDescriptor & 0xFF;

		float text_size_y = lnList.size() * h;
		float line_size_x = ImGui::CalcTextSize(pText + line_begin_index, pText + line_end_index).x;

		// Centered horizontal and vertical alignment
		cursor.y = p.y + (lt.y + rb.y) / 2 - text_size_y / 2 + i * h;
		cursor.x = p.x + (lt.x + rb.x) / 2 - line_size_x / 2;

		window->DrawList->AddText(cursor, ImGui::GetColorU32(ImGuiCol_Text), pText + line_begin_index, pText + line_end_index);
	}
}

static void DrawProgress()
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;

	float progress_global = (float)progress_pos / progress_limit;
	float progress_global_saturated = std::clamp(progress_global, 0.0f, 1.0f);
	char buf_global[32];
	sprintf(buf_global, "%d%%", (int)(progress_global_saturated * 100.0f));

	auto& [pos, size] = iap_config.gResourceLayoutMap[RSRC_PROGRESS];

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 1.0f, 0.0f, 0.5f));
	RDRProgress(progress_global, pos, size);
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	TextCentered(buf_global, pos, pos + size);
	ImGui::PopStyleColor();
}

static void DrawTips()
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	ImVec2 p = window->DC.CursorPos;

	auto& [pos, size] = iap_config.gResourceLayoutMap[RSRC_TIPS];

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	TextCentered(label_message, pos, pos + size);
	ImGui::PopStyleColor();
}

static void ShowRootWindowFileCorruption(void)
{
	ImVec2 pos(0.0f, 0.0f);
	ImVec2 size(600.0f, 400.0f);
	TextCentered(label_message, pos, pos + size);
	utils::AlertEx(&alert_flag, "MSG", alert_message);
}

static void ShowRootWindowProgress_v2(void)
{
	DrawImage(RSRC_IMG_BG);
	DrawImage(RSRC_IMG_LOGO);
	DrawImage(RSRC_IMG_VID);
	DrawImage(RSRC_IMG_PID);
	DrawImage(RSRC_IMG_VER);

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	ImGui::SetWindowFontScale(0.8f);
	{
		DrawImage(RSRC_IMG_TEXT_INPUT_VID);
		auto& [pos, size] = iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VID];
		TextCentered(deviceVID, pos, pos + size);
	}
	{
		DrawImage(RSRC_IMG_TEXT_INPUT_PID);
		auto& [pos, size] = iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_PID];
		TextCentered(devicePID, pos, pos + size);
	}
	{
		DrawImage(RSRC_IMG_TEXT_INPUT_VER);
		auto& [pos, size] = iap_config.gResourceLayoutMap[RSRC_IMG_TEXT_INPUT_VER];
		TextCentered(deviceVersion, pos, pos + size);
	}
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	DrawProgress();
	DrawTips();

#define DISABLED_BEGIN(test) if ((test)) ImGui::BeginDisabled();
#define DISABLED_END(test) if ((test)) ImGui::EndDisabled();

	bool disableDevInfo = g_main_state != MAINSTATE_IDLE;
	bool disableStart = g_main_state != MAINSTATE_IDLE || !g_file_valid_flag;
	bool disableAutoStart = g_main_state != MAINSTATE_IDLE && g_main_state != MAINSTATE_AUTO_IAP_MODE_IDLE;

	DISABLED_BEGIN(disableDevInfo);
	if (RDRImageButton("DEVINFO", RSRC_IMG_DEVINFO, RSRC_IMG_DEVINFO_HOVERED, RSRC_IMG_DEVINFO_ACTIVE)) {
		pool->enqueue(DeviceInfoThread);
	}
	DISABLED_END(disableDevInfo);

	DISABLED_BEGIN(disableStart);
	if (RDRImageButton("START", RSRC_IMG_START, RSRC_IMG_START_HOVERED, RSRC_IMG_START_ACTIVE))
	{
		pool->enqueue(IAPThread);
	}
	DISABLED_END(disableStart);

	DISABLED_BEGIN(disableAutoStart);
	bool last = g_auto_start;
	if (RDRImageButton("AUTOSTART", RSRC_IMG_AUTO_START, RSRC_IMG_AUTO_START_HOVERED, RSRC_IMG_AUTO_START_ACTIVE)) {
		g_auto_start = !g_auto_start;
	}
	RDRImageCheckbox("AUTO_START_CHECKBOX", RSRC_IMG_BOX, RSRC_IMG_BOX_CHECKED, &g_auto_start);
	if (g_auto_start != last) {
		pool->enqueue(AutoIAPThread);
	}
	DISABLED_END(disableAutoStart);
}

static void ShowRootWindow(bool* p_open)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

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

	//ShowRootWindowProgress();
	if (!g_file_valid_flag) {
		ShowRootWindowFileCorruption();
	} else {
		ShowRootWindowProgress_v2();
	}

	ImGui::End();
}

int main_gui()
{
	ShowRootWindow(&show_root_window);

	return 0;
}

