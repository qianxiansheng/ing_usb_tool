#include "memedit.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui/extensions/imgui_memory_editor.h"

MemoryEditor mem_edit;
uint8_t* p_mem = NULL;
size_t s_mem;

void ShowBinViewer(bool* p_open)
{
	mem_edit.DrawWindow("Binary Viewer", p_mem, s_mem);
}