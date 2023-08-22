#pragma once

#define APP_TITLE           "Ingchips USB IAP tool"
#define DEFAULT_FONT		"c:\\Windows\\Fonts\\msyh.ttc"

#define EP_MAX_SIZE  64
#define DATA_INPUT_BUFF_MAX_SIZE 256
#define BIN_NAME_BUFF_MAX_SIZE 128

#define BIN_CONFIG_DEFAULT_IDENTIFY "INGCHIPS"

#define ImGuiDCXAxisAlign(v) ImGui::SetCursorPos(ImVec2((v), ImGui::GetCursorPos().y))
#define ImGuiDCYMargin(v) ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + (v)))
