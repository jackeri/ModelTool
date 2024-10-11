#include "tools.h"

void mt::tools::printGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		// FIXME: the gluErrorString does not exists with Glad so fix this somehow
		// spdlog::get(MT_LOGGER)->error("OpenGL error: {}", reinterpret_cast<const char *>(gluErrorString(err)));
		spdlog::get(MT_LOGGER)->error("OpenGL error");
	}
}
