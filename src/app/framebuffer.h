#pragma once

#include "mt.h"

namespace mt {

	/*! Opengl framebuffer object wrapper */
	class Framebuffer {
	  public:
		Framebuffer() = default;

		~Framebuffer();

		/**
		 * Create or resize a framebuffer
		 * @param width width of the buffer
		 * @param height height of the buffer
		 */
		void create(GLint width, GLint height);

		/**
		 * Clear framebuffer memory and delete the object
		 */
		void clear();

		/**
		 * Bind this buffer as the render target
		 */
		void bind() const;

		/**
		 * Unbind this buffer and return to 0 - buffer
		 */
		void unbind() const;

		/**
		 * Get the OpenGL texture id used by this framebuffer
		 * @return texture id
		 */
		GLuint texture();

	  private:
		GLuint fboId = 0;	 ///< Framebuffer ID
		GLuint texId = 0;	 ///< Texture ID
		GLuint depthId = 0;	 ///< Depth texture ID
		GLint curWidth = 0;	 ///< Buffer current width
		GLint curHeight = 0; ///< Buffer current height
	};
}
