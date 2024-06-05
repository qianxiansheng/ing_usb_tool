#pragma once

#include "pch.h"
#include <iostream>
#include <filesystem>

#include "imgui.h"

struct app_settings_t
{
    bool opt_fullscreen;
    bool opt_showdemowindow;
    bool opt_showlogwindow;
    bool opt_showusbdevicetreewindow;
    bool opt_showusbsendwindow;
    bool opt_showbingenwindow;
    bool opt_showbinviewerwindow;
    bool opt_showiapwindow;

    bool opt_bingen_base_show;
    bool opt_bingen_base_expand;
    bool opt_bingen_base_chipcode_show;
    bool opt_bingen_base_projcode_show;
    bool opt_bingen_base_hardversion_show;
    bool opt_bingen_base_softversion_show;
    bool opt_bingen_base_blocksize_show;
    bool opt_bingen_base_upgradetype_show;

    // base
    bool opt_bingen_check_show;
    bool opt_bingen_check_expand;
    bool opt_bingen_encryption_show;
    bool opt_bingen_encryption_expand;
    bool opt_bingen_configexe_show;
    bool opt_bingen_configexe_expand;
    bool opt_bingen_choosebin_show;
    bool opt_bingen_choosebin_expand;

    // config exe
    bool opt_bingen_exe_tree_node_vid_pid_show;
    bool opt_bingen_exe_tree_node_options_show;
    bool opt_bingen_exe_tree_node_icon_show;
    bool opt_bingen_exe_tree_node_resource_show;
    bool opt_bingen_exe_tree_node_style_show;
};

void SaveSetting(std::filesystem::path ini_path);

void LoadSetting(std::filesystem::path ini_path);

