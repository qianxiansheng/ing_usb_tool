#pragma once

#include <filesystem>

struct iap_config_t
{
	uint16_t boot_vid;
	uint16_t boot_pid;
	uint8_t  boot_rid;

	uint16_t app_vid;
	uint16_t app_pid;
	uint8_t  app_rid;

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