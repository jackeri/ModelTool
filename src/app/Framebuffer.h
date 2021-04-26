#pragma once

#include "mt.h"

namespace mt {
	class Framebuffer {
	public:
		Framebuffer() = default;

		~Framebuffer();

		void create(GLint width, GLint height);

		void clear();

		void bind() const;

		void unbind() const;

	private:
		GLuint fboId = 0;
		GLuint texId = 0;
		GLuint depthId = 0;
		GLint curWidth = 0;
		GLint curHeight = 0;
	};
}
