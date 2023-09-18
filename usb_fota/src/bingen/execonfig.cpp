#include "execonfig.h"

#include <iostream>
#include <fstream>
#include "util/INIReader.h"
#include "util/utils.h"

exe_config_t exe_config = { 0 };

#define EXE_CONFIG_DEFAULT_BOOT_VID "36B0"
#define EXE_CONFIG_DEFAULT_BOOT_PID "0101"
#define EXE_CONFIG_DEFAULT_BOOT_RID "3F"
#define EXE_CONFIG_DEFAULT_APP_VID  "36B0"
#define EXE_CONFIG_DEFAULT_APP_PID  "0102"
#define EXE_CONFIG_DEFAULT_APP_RID  "2F"

#define EXE_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT  8000
#define EXE_CONFIG_DEFAULT_READ_ACK_TIMEOUT 1000
#define EXE_CONFIG_DEFAULT_REBOOT_DELAY  1000
#define EXE_CONFIG_DEFAULT_SWITCH_DELAY  1000
#define EXE_CONFIG_DEFAULT_RETRY_NUM  3

void SaveEXEConfig(std::filesystem::path ini_path)
{
	exe_config_t& c = exe_config;

	std::ofstream out;

	out.open(ini_path, std::ios::out);
	out << "[option]" << std::endl;
	out << "retryNum=" << c.retryNum << std::endl;
	out << "rebootDelay=" << c.rebootDelay << std::endl;
	out << "switchDelay=" << c.switchDelay << std::endl;
	out << "searchDeviceTimeout=" << c.searchDeviceTimeout << std::endl;
	out << "readAckTimeout=" << c.readAckTimeout << std::endl;
	out << std::endl;

	out << "[boot]" << std::endl;
	out << "vid="      << c.tbvid << std::endl;
	out << "pid="      << c.tbpid << std::endl;
	out << "reportid=" << c.tbrid << std::endl;
	out << std::endl;

	out << "[app]" << std::endl;
	out << "vid="      << c.tavid << std::endl;
	out << "pid="      << c.tapid << std::endl;
	out << "reportid=" << c.tarid << std::endl;
	out << std::endl;

	out << std::endl;
	out.close();
}

void LoadEXEConfig(std::filesystem::path ini_path)
{
	exe_config_t& c = exe_config;

	INIReader reader(ini_path.u8string());

	c.retryNum    = reader.GetInteger("option", "retryNum", EXE_CONFIG_DEFAULT_RETRY_NUM);
	c.rebootDelay = reader.GetInteger("option", "rebootDelay", EXE_CONFIG_DEFAULT_REBOOT_DELAY);
	c.switchDelay = reader.GetInteger("option", "switchDelay", EXE_CONFIG_DEFAULT_SWITCH_DELAY);
	c.searchDeviceTimeout = reader.GetInteger("option", "searchDeviceTimeout", EXE_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT);
	c.readAckTimeout = reader.GetInteger("option", "readAckTimeout", EXE_CONFIG_DEFAULT_READ_ACK_TIMEOUT);

	strcpy(c.tbvid, reader.Get("boot", "vid", EXE_CONFIG_DEFAULT_BOOT_VID).c_str());
	strcpy(c.tbpid, reader.Get("boot", "pid", EXE_CONFIG_DEFAULT_BOOT_PID).c_str());
	strcpy(c.tbrid, reader.Get("boot", "reportid", EXE_CONFIG_DEFAULT_BOOT_RID).c_str());

	utils::ValidateU16Text(c.tbvid, c.boot_vid);
	utils::ValidateU16Text(c.tbpid, c.boot_pid);
	utils::ValidateU8Text (c.tbrid, c.boot_rid);

	strcpy(c.tavid, reader.Get("app", "vid"     , EXE_CONFIG_DEFAULT_APP_VID).c_str());
	strcpy(c.tapid, reader.Get("app", "pid"     , EXE_CONFIG_DEFAULT_APP_PID).c_str());
	strcpy(c.tarid, reader.Get("app", "reportid", EXE_CONFIG_DEFAULT_APP_RID).c_str());

	utils::ValidateU16Text(c.tavid, c.app_vid);
	utils::ValidateU16Text(c.tapid, c.app_pid);
	utils::ValidateU8Text (c.tarid, c.app_rid);
}

void SetEXEConfigDefault()
{
	exe_config_t& c = exe_config;

	c.retryNum = EXE_CONFIG_DEFAULT_RETRY_NUM;
	c.rebootDelay = EXE_CONFIG_DEFAULT_REBOOT_DELAY;
	c.switchDelay = EXE_CONFIG_DEFAULT_SWITCH_DELAY;
	c.searchDeviceTimeout = EXE_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT;
	c.readAckTimeout = EXE_CONFIG_DEFAULT_READ_ACK_TIMEOUT;

	c.boot_vid = utils::htoi_16(EXE_CONFIG_DEFAULT_BOOT_VID);
	c.boot_pid = utils::htoi_16(EXE_CONFIG_DEFAULT_BOOT_PID);
	c.boot_rid = utils::htoi_8(EXE_CONFIG_DEFAULT_BOOT_RID);

	c.app_vid = utils::htoi_16(EXE_CONFIG_DEFAULT_APP_VID);
	c.app_pid = utils::htoi_16(EXE_CONFIG_DEFAULT_APP_PID);
	c.app_rid = utils::htoi_8(EXE_CONFIG_DEFAULT_APP_RID);
}
