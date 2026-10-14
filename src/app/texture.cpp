#include "texture.h"

#include <utility>

#include "image.h"

mt::Texture::Texture(std::string name) : name(std::move(name)) {}

mt::Texture::~Texture()
{
	if (textId)
	{
		glDeleteTextures(1, &textId);
		textId = 0;
	}
}

bool mt::Texture::decode(const byte *data, size_t length, io::Image &image)
{
	return io::decodeImage(data, length, image);
}

bool mt::Texture::upload(const io::Image &image, TextureFilter filter)
{
	if (image.width <= 0 || image.height <= 0 || image.pixels.size() < size_t(image.width) * size_t(image.height) * 4)
	{
		return false;
	}

	if (!textId)
	{
		glGenTextures(1, &textId);
	}

	width = image.width;
	height = image.height;
	alpha = false;
	for (size_t i = 3; i < image.pixels.size(); i += 4)
	{
		if (image.pixels[i] < 255)
		{
			alpha = true;
			break;
		}
	}

	bind();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	switch (filter)
	{
	case TextureFilter::Nearest:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		break;
	case TextureFilter::Linear:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		break;
	case TextureFilter::Trilinear:
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		break;
	}

	unbind();
	return true;
}

void mt::Texture::bind() const
{
	glBindTexture(GL_TEXTURE_2D, textId);
}

void mt::Texture::setClamp(bool clamp) const
{
	glBindTexture(GL_TEXTURE_2D, textId);
	const GLint wrap = clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void mt::Texture::unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec2 mt::Texture::size() const
{
	return glm::vec2(width, height);
}

GLuint mt::Texture::id() const
{
	return textId;
}
