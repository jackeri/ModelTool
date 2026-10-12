#pragma once

#include "mt.h"
#include "texture_lookup.h"

namespace mt {

	/*! Texture filtering, like the game's texture modes */
	enum class TextureFilter
	{
		Nearest,  ///< no filtering (GL_NEAREST)
		Linear,	  ///< bilinear (GL_LINEAR)
		Trilinear ///< bilinear with mipmaps (GL_LINEAR_MIPMAP_LINEAR)
	};

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

		Texture(const Texture &) = delete;
		Texture &operator=(const Texture &) = delete;

		/**
		 * Decode an image file (tga, jpg, png, ...)
		 * @param data file data
		 * @param length length of the data
		 * @param image decoded RGBA image
		 * @return true if the image could be decoded
		 */
		static bool decode(const byte *data, size_t length, io::Image &image);

		/**
		 * Upload an image into the texture
		 * @param image RGBA image
		 * @param filter texture filtering
		 * @return true if the upload succeeded
		 */
		bool upload(const io::Image &image, TextureFilter filter);

		/**
		 * bind the current texture
		 */
		void bind() const;

		/**
		 * unbind texture (binds 0 as the current texture)
		 */
		static void unbind();

		/**
		 * Returns the size of the texture
		 * @return texture size
		 */
		[[nodiscard]] glm::vec2 size() const;

		/**
		 * Returns the OpenGL texture id
		 * @return texture id
		 */
		[[nodiscard]] GLuint id() const;

		/**
		 * @return true if the image has transparent pixels
		 */
		[[nodiscard]] bool hasAlpha() const
		{
			return alpha;
		}

	  private:
		int width = -1;		///< texture width
		int height = -1;	///< texture height
		bool alpha = false; ///< has transparent pixels
		std::string name{}; ///< texture name
		GLuint textId = 0;	///< texture OpenGL id
	};
}
