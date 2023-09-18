#include "config.h"

#include <iostream>
#include <fstream>
#include "util/INIReader.h"
#include "util/utils.h"

iap_config_t iap_config = { };

#define IAP_CONFIG_DEFAULT_BOOT_VID "36B0"
#define IAP_CONFIG_DEFAULT_BOOT_PID "0101"
#define IAP_CONFIG_DEFAULT_BOOT_RID "3F"
#define IAP_CONFIG_DEFAULT_APP_VID  "36B0"
#define IAP_CONFIG_DEFAULT_APP_PID  "0102"
#define IAP_CONFIG_DEFAULT_APP_RID  "2F"

#define IAP_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT  8000
#define IAP_CONFIG_DEFAULT_READ_ACK_TIMEOUT 1000
#define IAP_CONFIG_DEFAULT_REBOOT_DELAY  1000
#define IAP_CONFIG_DEFAULT_SWITCH_DELAY  1000
#define IAP_CONFIG_DEFAULT_RETRY_NUM  3

static std::filesystem::path ini_path = std::filesystem::current_path().concat("\\config.ini");

void Saveini(std::filesystem::path ini_path)
{
	iap_config_t& c = iap_config;

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
	out << "vid="      << std::hex << c.boot_vid << std::dec << std::endl;
	out << "pid="      << std::hex << c.boot_pid << std::dec << std::endl;
	out << "reportid=" << std::hex << (int)c.boot_rid << std::dec << std::endl;
	out << std::endl;

	out << "[app]" << std::endl;
	out << "vid="      << std::hex << c.app_vid << std::dec << std::endl;
	out << "pid="      << std::hex << c.app_pid << std::dec << std::endl;
	out << "reportid=" << std::hex << (int)c.app_rid << std::dec << std::endl;
	out << std::endl;

	out << std::endl;
	out.close();
}

void Loadini(std::filesystem::path ini_path)
{
	iap_config_t& c = iap_config;

	INIReader reader(ini_path.u8string());

	c.retryNum    = reader.GetInteger("option", "retryNum", IAP_CONFIG_DEFAULT_RETRY_NUM);
	c.rebootDelay = reader.GetInteger("option", "rebootDelay", IAP_CONFIG_DEFAULT_REBOOT_DELAY);
	c.switchDelay = reader.GetInteger("option", "switchDelay", IAP_CONFIG_DEFAULT_SWITCH_DELAY);
	c.searchDeviceTimeout = reader.GetInteger("option", "searchDeviceTimeout", IAP_CONFIG_DEFAULT_SEARCH_DEVICE_TIMEOUT);
	c.readAckTimeout = reader.GetInteger("option", "readAckTimeout", IAP_CONFIG_DEFAULT_READ_ACK_TIMEOUT);

	utils::ValidateU16Text((char*)reader.Get("boot", "vid"     , IAP_CONFIG_DEFAULT_BOOT_VID).c_str(), c.boot_vid);
	utils::ValidateU16Text((char*)reader.Get("boot", "pid"     , IAP_CONFIG_DEFAULT_BOOT_PID).c_str(), c.boot_pid);
	utils::ValidateU8Text ((char*)reader.Get("boot", "reportid", IAP_CONFIG_DEFAULT_BOOT_RID).c_str(), c.boot_rid);
	
	utils::ValidateU16Text((char*)reader.Get("app", "vid"     , IAP_CONFIG_DEFAULT_APP_VID).c_str(), c.app_vid);
	utils::ValidateU16Text((char*)reader.Get("app", "pid"     , IAP_CONFIG_DEFAULT_APP_PID).c_str(), c.app_pid);
	utils::ValidateU8Text ((char*)reader.Get("app", "reportid", IAP_CONFIG_DEFAULT_APP_RID).c_str(), c.app_rid);
}

void LoadConfigINI()
{
	Loadini(ini_path);
	if (!std::filesystem::exists(ini_path))
		Saveini(ini_path);
}