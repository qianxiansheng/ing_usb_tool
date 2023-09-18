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
#include "util/myqueue.h"

#include "config.h"
#include "usb.h"
#include "iap.h"

#define IMIDTEXT(name, i) ((std::string(name) + std::to_string(i)).c_str())

static bool show_root_window = true;
static bool opt_fullscreen = true;

static bool alert_flag = false;
static char alert_message[256] = { 0 };
static char label_message[256] = { 0 };

static bool g_upgrading_flag = false;
static bool g_file_valid_flag = false;

uint32_t progress_limit = 100;
uint32_t progress_pos = 100;

std::filesystem::path selfName;

std::vector<uint8_t> iap_bin;

extern iap_config_t iap_config;

ImVec4 clear_color = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

ThreadPool* pool;

enum UIEventTypeEnum
{
	UI_EVENT_TYPE_ALERT_MESSAGE = 0x30,
	UI_EVENT_TYPE_LABEL_MESSAGE = 0x31,
	UI_EVENT_TYPE_EXIT = 0xF0,
};

typedef struct
{
	uint32_t eventType;

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


static void SearchDevice(uint16_t vid, uint16_t pid, uint8_t rid, uint32_t milliseconds, HIDDevice* dev)
{
	auto overTime = std::chrono::system_clock::now() + 
		std::chrono::milliseconds(milliseconds);

	while (std::chrono::system_clock::now() < overTime)
	{
		if (OpenHIDInterface(vid, pid, rid, dev) == HID_FIND_SUCCESS)
		{
			printf("report ID:%02X\n", dev->reportId);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
static void SearchDeviceBootOrAPP(uint16_t bvid, uint16_t bpid, uint16_t avid, uint16_t apid, uint32_t milliseconds, HIDDevice* dev)
{
	auto overTime = std::chrono::system_clock::now() +
		std::chrono::milliseconds(milliseconds);

	while (std::chrono::system_clock::now() < overTime)
	{
		if (OpenHIDInterface(bvid, bpid, dev) == HID_FIND_SUCCESS)
		{
			printf("report ID:%02X\n", dev->reportId);
			break;
		}
		if (OpenHIDInterface(avid, apid, dev) == HID_FIND_SUCCESS)
		{
			printf("report ID:%02X\n", dev->reportId);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
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

static void LoadData()
{
	uint32_t size = (uint32_t)std::filesystem::file_size(selfName);
	printf("exe size:%d\n", size);

	std::vector<uint8_t> self = utils::readFileData(selfName);

	uint32_t N = 0;
	uint32_t N_ = 0;

	uint8_t* self_data = self.data();
	uint32_t self_size = (uint32_t)self.size();
	uint32_t k = self_size;
	k -= 4;
	if (takeout_32_little(&self_data[k]) == IAP_GEN_EXE_SUFFIX)
	{
		k -= 4;
		N = takeout_32_little(&self_data[k]);
		N_ = self_size - N - 38;
		k -= 4; iap_config.readAckTimeout = takeout_32_little(&self_data[k]);
		k -= 4; iap_config.searchDeviceTimeout = takeout_32_little(&self_data[k]);
		k -= 4; iap_config.switchDelay = takeout_32_little(&self_data[k]);
		k -= 4; iap_config.rebootDelay = takeout_32_little(&self_data[k]);
		k -= 4; iap_config.retryNum = takeout_32_little(&self_data[k]);
		k -= 1; iap_config.app_rid = takeout_8_little(&self_data[k]);
		k -= 2; iap_config.app_pid = takeout_16_little(&self_data[k]);
		k -= 2; iap_config.app_vid = takeout_16_little(&self_data[k]);
		k -= 1; iap_config.boot_rid = takeout_8_little(&self_data[k]);
		k -= 2; iap_config.boot_pid = takeout_16_little(&self_data[k]);
		k -= 2; iap_config.boot_vid = takeout_16_little(&self_data[k]);

		printf("boot vid:0x%04X\n", iap_config.boot_vid);
		printf("boot pid:0x%04X\n", iap_config.boot_pid);
		printf("boot rid:0x%04X\n", iap_config.boot_rid);
		printf("app  vid:0x%04X\n", iap_config.app_vid);
		printf("app  pid:0x%04X\n", iap_config.app_pid);
		printf("app  rid:0x%04X\n", iap_config.app_rid);
		printf("retryNum:%d\n", iap_config.retryNum);
		printf("rebootDelay:%d\n", iap_config.rebootDelay);
		printf("switchDelay:%d\n", iap_config.switchDelay);
		printf("searchDeviceTimeout:%d\n", iap_config.searchDeviceTimeout);
		printf("readAckTimeout:%d\n", iap_config.readAckTimeout);


		iap_bin.resize(N_);
		memcpy(iap_bin.data(), self_data + N, N_);
		g_file_valid_flag = true;
	}
	else
	{
		UIThreadPutEvent_LabelMessage("File corruption!\nPlease regenerate the upgrade tool.");
		UIThreadPutEvent_AlertMessage("File corruption!\nPlease regenerate the upgrade tool.");
		g_file_valid_flag = false;
	}
}

static void LoadData0()
{
	std::filesystem::path binName("C:\\Users\\leosh\\Desktop\\USB tool\\output\\INGIAP.bin");
	auto data = utils::readFileData(binName);

	iap_config.readAckTimeout = 1000;
	iap_config.searchDeviceTimeout = 8000;
	iap_config.switchDelay = 1000;
	iap_config.rebootDelay = 1000;
	iap_config.retryNum = 3;
	iap_config.app_rid = 0x2F;
	iap_config.app_pid = 0x0102;
	iap_config.app_vid = 0x36B0;
	iap_config.boot_rid = 0x3F;
	iap_config.boot_pid = 0x0101;
	iap_config.boot_vid = 0x36B0;

	iap_bin = data;
	g_file_valid_flag = true;
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
static void IAPThread()
{
	showLabel("Open Device...");
	HIDDevice hid = { 0 };
	SearchDeviceBootOrAPP(
		iap_config.boot_vid, iap_config.boot_pid,
		iap_config.app_vid,  iap_config.app_pid,
		iap_config.searchDeviceTimeout, &hid);

	uint8_t* header = iap_bin.data();
	uint16_t block_size = header[66] + (header[67] << 8);

	bool openAPP = false;

	if (hid.phandle == NULL)
	{
		showLabel("Open Failed!");
		g_upgrading_flag = false;
		return;
	}
	else
	{
		if (hid.reportId == iap_config.boot_rid)
		{
			showLabel("Open Success! BOOT");
		}
		else
		{
			openAPP = true;
			showLabel("Open Success! APP");
		}
	}

	if (openAPP)
	{
		InitIAPContext(hid, iap_bin, block_size, 
			iap_config.retryNum, 
			iap_config.switchDelay,
			iap_config.switchDelay,
			onbusinessOk, &iap_ctx);
		iap_run_switch_boot(iap_ctx);
		if (iap_ctx.primary_status == IAP_STATUS_COMPLETE)
		{
			CloseHIDInterface(hid);
			hid.phandle = NULL;
			showLabel("Send switch BOOT Success.");
			SearchDevice(iap_config.boot_vid, iap_config.boot_pid, 
				iap_config.boot_rid, iap_config.searchDeviceTimeout, &hid);

			if (hid.phandle == NULL)
			{
				showAlert_("Open BOOT Failed!");
				g_upgrading_flag = false;
				return;
			}
			else
			{
				showLabel("Open Success! BOOT");
			}
		}
		else
		{
			CloseHIDInterface(hid);
			showAlert_("Send switch APP Failed.");
			g_upgrading_flag = false;
			return;
		}
	}

	InitIAPContext(hid, iap_bin, block_size, 
		iap_config.retryNum, 
		iap_config.readAckTimeout, 
		iap_config.switchDelay, 
		onbusinessOk, &iap_ctx);
	try {
		iap_run(iap_ctx);
	} catch (std::exception) {
		showAlert_("The device has been lost.");
		CloseHIDInterface(hid);
		g_upgrading_flag = false;
		return;
	}
	if (iap_ctx.primary_status == IAP_STATUS_COMPLETE)
	{
		showAlert_("Upgrade completed.");
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
	g_upgrading_flag = false;
}

static bool main_init(int argc, char* argv[])
{
	ImGuiIO& io = ImGui::GetIO();
	// disable imgui.ini
	io.IniFilename = NULL;
	io.LogFilename = NULL;

	selfName = argv[0];

	// Theme
	ImGui::StyleColorsLight();

	// Font
	io.Fonts->AddFontFromFileTTF(DEFAULT_FONT, 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

	// Thread pool
	pool = new ThreadPool(10);

	// UI thread
	queueUI = new BlockingQueue<UIEvent>(32);
	pool->enqueue(UIThread);

	// IAP bin data
	LoadData();

	// HID
	HIDInit();

	return true;
}

extern bool stop;
static void main_shutdown(void)
{
	UIThreadPutEvent_Exit();
	stop = true;
	delete pool;
	HIDExit();
}

static void ShowRootWindowProgress(void)
{
	ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();

	ImGui::Text(label_message);

	const float progress_global_size = contentRegionAvail.x;
	float progress_global = (float)progress_pos / progress_limit;
	float progress_global_saturated = std::clamp(progress_global, 0.0f, 1.0f);
	char buf_global[32];
	sprintf(buf_global, "%d/%d", (int)(progress_global_saturated * progress_limit), progress_limit);
	ImGui::ProgressBar(progress_global, ImVec2(progress_global_size, 0.f), buf_global);

	if (g_upgrading_flag || !g_file_valid_flag)
	{
		ImGui::BeginDisabled();
		ImGui::Button("Start", ImVec2(contentRegionAvail.x, 50.0f));
		ImGui::EndDisabled();
	}
	else
	{
		if (ImGui::Button("Start", ImVec2(contentRegionAvail.x, 50.0f)))
		{
			g_upgrading_flag = true;
			pool->enqueue(IAPThread);
		}
	}
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

	ShowRootWindowProgress();

	utils::AlertEx(&alert_flag, "MSG", alert_message);

	ImGui::End();
}

static int main_gui()
{
	ShowRootWindow(&show_root_window);

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