#pragma once

#include <filesystem>

struct exe_config_t
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

	char tbvid[5];
	char tbpid[5];
	char tbrid[3];
	char tavid[5];
	char tapid[5];
	char tarid[3];
};

void SaveEXEConfig(std::filesystem::path ini_path);
void LoadEXEConfig(std::filesystem::path ini_path);

void SetEXEConfigDefault();
