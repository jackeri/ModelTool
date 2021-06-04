#include "framebuffer.h"
#include "tools.h"

mt::Framebuffer::~Framebuffer()
{
	clear();
}

void mt::Framebuffer::create(GLint width, GLint height)
{
	if (fboId)
	{
		if (width == curWidth && height == curHeight)
		{
			return;
		}

		clear();
	}

	curWidth = width;
	curHeight = height;

	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);

	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, curWidth, curHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);

	glGenTextures(1, &depthId);
	glBindTexture(GL_TEXTURE_2D, depthId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, curWidth, curHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthId, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		tools::printGLErrors();
	}

	unbind();
}

void mt::Framebuffer::clear()
{
	if (fboId)
	{
		glDeleteFramebuffers(GL_FRAMEBUFFER, &fboId);
		glDeleteTextures(1, &texId);
		glDeleteTextures(1, &depthId);
		texId = 0;
		depthId = 0;
		fboId = 0;
	}
}

void mt::Framebuffer::bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glViewport(0, 0, curWidth, curHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void mt::Framebuffer::unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint mt::Framebuffer::texture()
{
	return texId;
}
