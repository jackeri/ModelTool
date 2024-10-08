#include "tools.h"

void mt::tools::printGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		spdlog::get(MT_LOGGER)->error("OpenGL error: {}", reinterpret_cast<const char *>(gluErrorString(err)));
	}
}
