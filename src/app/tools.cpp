#include "tools.h"

namespace {
	const char *glErrorName(GLenum error)
	{
		switch (error)
		{
		case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		default: return "unknown error";
		}
	}
}

void mt::tools::printGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		spdlog::get(MT_LOGGER)->error("OpenGL error: {} (0x{:x})", glErrorName(err), err);
	}
}
