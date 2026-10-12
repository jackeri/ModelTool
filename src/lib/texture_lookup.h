#pragma once

#include "library.h"
#include "files.h"

namespace mt::io {

	/*! Image extensions tried when looking up a texture, in order (the game looks for .tga and .jpg) */
	const std::vector<std::string> &textureExtensions();

	/**
	 * Paths to try for a material's texture: the name as it is, without its leading folders (for absolute paths like
	 * "C:/mapping/etmain/models/..."), and next to the model, each first with its own extension and then with every
	 * image extension (stacked extensions like "Head.tga.png" are removed first)
	 * @param material material or texture name (backslashes are allowed)
	 * @param modelPath path of the model that uses the material, can be empty
	 * @return candidate paths without duplicates
	 */
	std::vector<std::string> textureCandidates(const std::string &material, const std::string &modelPath);

	/**
	 * Find the texture of a material
	 * @param source file source to search
	 * @param material material or texture name
	 * @param modelPath path of the model that uses the material, can be empty
	 * @return path of the texture, empty if not found
	 */
	std::string findTexture(FileSource &source, const std::string &material, const std::string &modelPath);

	/*! RGBA image */
	struct Image {
		int width = 0;
		int height = 0;
		std::vector<byte> pixels{}; ///< width * height * 4 bytes
	};

	/**
	 * Halve the image size picmip times like the game's r_picmip, but not below minSize
	 * @param image image to reduce
	 * @param picmip number of halvings
	 * @param minSize smallest width or height to reduce to
	 * @return reduced image
	 */
	Image applyPicmip(const Image &image, int picmip, int minSize = 16);
}
