#include "itexture.h"

#include "glad/glad.h"

#ifdef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#endif
#include "stb_image.h"

ITexture::ITexture(std::vector<uint8_t> data) {
	const stbi_uc* buffer = data.data();
	int len = static_cast<int>(data.size());

	int width, height, numChannels;
	unsigned char* imageData = stbi_load_from_memory(buffer, len, &width, &height, &numChannels, STBI_rgb_alpha);
	if (!imageData)
		throw std::exception("failed to load image data!");

	int req_channels = 4;
	int size = width * height * req_channels;

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

	_size.x = static_cast<float>(width);
	_size.y = static_cast<float>(height);
	_texID = reinterpret_cast<void*>(static_cast<uintptr_t>(textureID));

	stbi_image_free(imageData);
}
ITexture::ITexture(std::filesystem::path path) {
	if (!std::filesystem::exists(path) ||
		!std::filesystem::is_regular_file(path)) {
		throw std::exception("file not exists!");
	}

	int width, height, numChannels;
	unsigned char* imageData = stbi_load(path.generic_string().c_str(), &width, &height, &numChannels, STBI_rgb_alpha);
	if (!imageData)
		throw std::exception("failed to load image data!");

	int req_channels = 4;
	int size = width * height * req_channels;

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

	_size.x = static_cast<float>(width);
	_size.y = static_cast<float>(height);
	_texID = reinterpret_cast<void*>(static_cast<uintptr_t>(textureID));

	stbi_image_free(imageData);
};
ITexture::~ITexture() {
	GLuint textureID = static_cast<GLuint>(reinterpret_cast<uintptr_t>(_texID));
	glDeleteTextures(1, &textureID);
};
