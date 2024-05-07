#include "setting.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include "util/INIReader.h"
#include "util/utils.h"

app_settings_t gSettings = {};


void SaveSetting(std::filesystem::path ini_path)
{
	app_settings_t& c = gSettings;

	std::ofstream out;
	out.open(ini_path, std::ios::out);

	out << "[settings]" << std::endl;
	out << "opt_fullscreen=" << c.opt_fullscreen << std::endl;
	out << "opt_showdemowindow=" << c.opt_showdemowindow << std::endl;
	out << "opt_showlogwindow=" << c.opt_showlogwindow << std::endl;
	out << "opt_showusbdevicetreewindow=" << c.opt_showusbdevicetreewindow << std::endl;
	out << "opt_showusbsendwindow=" << c.opt_showusbsendwindow << std::endl;
	out << "opt_showbingenwindow=" << c.opt_showbingenwindow << std::endl;
	out << "opt_showbinviewerwindow=" << c.opt_showbinviewerwindow << std::endl;
	out << "opt_showiapwindow=" << c.opt_showiapwindow << std::endl;
	out << "opt_bingen_base_show=" << c.opt_bingen_base_show << std::endl;
	out << "opt_bingen_base_expand=" << c.opt_bingen_base_expand << std::endl;
	out << "opt_bingen_check_show=" << c.opt_bingen_check_show << std::endl;
	out << "opt_bingen_check_expand=" << c.opt_bingen_check_expand << std::endl;
	out << "opt_bingen_base_chipcode_show=" << c.opt_bingen_base_chipcode_show << std::endl;
	out << "opt_bingen_base_projcode_show=" << c.opt_bingen_base_projcode_show << std::endl;
	out << "opt_bingen_base_hardversion_show=" << c.opt_bingen_base_hardversion_show << std::endl;
	out << "opt_bingen_base_softversion_show=" << c.opt_bingen_base_softversion_show << std::endl;
	out << "opt_bingen_base_blocksize_show=" << c.opt_bingen_base_blocksize_show << std::endl;
	out << "opt_bingen_base_upgradetype_show=" << c.opt_bingen_base_upgradetype_show << std::endl;

	out << "opt_bingen_encryption_show=" << c.opt_bingen_encryption_show << std::endl;
	out << "opt_bingen_encryption_expand=" << c.opt_bingen_encryption_expand << std::endl;
	out << "opt_bingen_configexe_show=" << c.opt_bingen_configexe_show << std::endl;
	out << "opt_bingen_configexe_expand=" << c.opt_bingen_configexe_expand << std::endl;
	out << "opt_bingen_choosebin_show=" << c.opt_bingen_choosebin_show << std::endl;
	out << "opt_bingen_choosebin_expand=" << c.opt_bingen_choosebin_expand << std::endl;
	out << std::endl;


	out.close();
}

void LoadSetting(std::filesystem::path ini_path)
{
	app_settings_t& c = gSettings;

	INIReader reader(ini_path.generic_string());

	c.opt_fullscreen = reader.GetBoolean("settings", "opt_fullscreen", true);
	c.opt_showdemowindow = reader.GetBoolean("settings", "opt_showdemowindow", false);
	c.opt_showlogwindow = reader.GetBoolean("settings", "opt_showlogwindow", false);
	c.opt_showusbdevicetreewindow = reader.GetBoolean("settings", "opt_showusbdevicetreewindow", false);
	c.opt_showusbsendwindow = reader.GetBoolean("settings", "opt_showusbsendwindow", false);
	c.opt_showbingenwindow = reader.GetBoolean("settings", "opt_showbingenwindow", true);
	c.opt_showbinviewerwindow = reader.GetBoolean("settings", "opt_showbinviewerwindow", false);
	c.opt_showiapwindow = reader.GetBoolean("settings", "opt_showiapwindow", false);
	c.opt_bingen_base_show = reader.GetBoolean("settings", "opt_bingen_base_show", true);
	c.opt_bingen_base_expand = reader.GetBoolean("settings", "opt_bingen_base_expand", true);
	c.opt_bingen_base_chipcode_show = reader.GetBoolean("settings", "opt_bingen_base_chipcode_show", false);
	c.opt_bingen_base_projcode_show = reader.GetBoolean("settings", "opt_bingen_base_projcode_show", false);
	c.opt_bingen_base_hardversion_show = reader.GetBoolean("settings", "opt_bingen_base_hardversion_show", false);
	c.opt_bingen_base_softversion_show = reader.GetBoolean("settings", "opt_bingen_base_softversion_show", false);
	c.opt_bingen_base_blocksize_show = reader.GetBoolean("settings", "opt_bingen_base_blocksize_show", false);
	c.opt_bingen_base_upgradetype_show = reader.GetBoolean("settings", "opt_bingen_base_upgradetype_show", true);
	c.opt_bingen_check_show = reader.GetBoolean("settings", "opt_bingen_check_show", false);
	c.opt_bingen_check_expand = reader.GetBoolean("settings", "opt_bingen_check_expand", false);
	c.opt_bingen_encryption_show = reader.GetBoolean("settings", "opt_bingen_encryption_show", false);
	c.opt_bingen_encryption_expand = reader.GetBoolean("settings", "opt_bingen_encryption_expand", false);
	c.opt_bingen_configexe_show = reader.GetBoolean("settings", "opt_bingen_configexe_show", true);
	c.opt_bingen_configexe_expand = reader.GetBoolean("settings", "opt_bingen_configexe_expand", true);
	c.opt_bingen_choosebin_show = reader.GetBoolean("settings", "opt_bingen_choosebin_show", true);
	c.opt_bingen_choosebin_expand = reader.GetBoolean("settings", "opt_bingen_choosebin_expand", true);
}

