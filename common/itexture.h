#ifndef __ITEXTURE_H__
#define __ITEXTURE_H__

#include <filesystem>
#include "imgui/imgui.h"

class ITexture
{
public:
	ITexture(std::vector<uint8_t> data);
	ITexture(std::filesystem::path path);
	~ITexture();
public:
	ImTextureID _texID;
	ImVec2 _size;
};

#endif