#pragma once

#include "mt.h"

namespace mt {

	/*! OpenGL texture wrapper */
	class Texture {
	public:

		/**
		 * Creates an texture with a name
		 * @param name name of the texture
		 */
		explicit Texture(std::string name);

		/**
		 * Free's the Opengl objects
		 */
		~Texture();

		/**
		 * Load file's image data into the opengl texture
		 * @param data file data
		 * @return true if the loading succeeded
		 */
		bool load(const byte_buffer& data);

		/**
		 * bind the current texture
		 */
		void bind() const;

		/**
		 * unbind texture (binds 0 as the current texture)
		 */
		void unbind();

		/**
		 * Returns the size of the texture
		 * @return texture size
		 */
		glm::vec2 size();

		/**
		 * Returns the OpenGL texture id
		 * @return texture id
		 */
		GLuint id();

	private:
		int width = -1; ///< texture width
		int height = -1; ///< texture height
		std::string name{}; ///< texture name
		GLuint textId = 0; ///< texture OpenGL id
	};
}
