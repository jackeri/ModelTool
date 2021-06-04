#include "Texture.h"

#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

mt::Texture::Texture(std::string name) : name(std::move(name))
{

}

mt::Texture::~Texture()
{
	if (textId)
	{
		glDeleteTextures(1, &textId);
		textId = 0;
	}
}

bool mt::Texture::load(const mt::byte_buffer& data)
{
	glGenTextures(1, &textId);
	bind();

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	int comp = 0;
	byte *imageData = stbi_load_from_memory(data->data(), data->size(), &width, &height, &comp, STBI_rgb_alpha);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

	STBI_FREE(imageData);

	return true;
}

void mt::Texture::bind() const
{
	glBindTexture(GL_TEXTURE_2D, textId);
}

void mt::Texture::unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec2 mt::Texture::size()
{
	return glm::vec2(width, height);
}

GLuint mt::Texture::id()
{
	return textId;
}
