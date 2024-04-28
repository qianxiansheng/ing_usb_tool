#pragma once

#include <filesystem>
#include "imgui.h"

struct iap_config_t
{
	uint16_t boot_vid;
	uint16_t boot_pid;
	uint8_t  boot_rid;

	uint16_t app_vid;
	uint16_t app_pid;
	uint8_t  app_rid;

	ImVec4 color0;
	ImVec4 color1;
	ImVec4 color2;
	ImVec4 color3;
	ImVec4 color4;
	ImVec4 color5;
	ImVec4 color6;
	ImVec4 color7;
	ImVec4 color8;
	ImVec4 color9;

	int retryNum;
	int rebootDelay;
	int switchDelay;
	int searchDeviceTimeout;
	int readAckTimeout;
};

void Saveini(std::filesystem::path ini_path);
void Loadini(std::filesystem::path ini_path);

void LoadConfigINI();
void SaveConfigINI();