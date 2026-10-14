#pragma once

#include "library.h"
#include "texture_lookup.h"

namespace mt::io {

	/**
	 * Decode an image file (tga, jpg, png, bmp, ...) into RGBA pixels, the first row is the top of the image
	 * @param data file data
	 * @param length file length
	 * @param image decoded image
	 * @return false if the data is not an image
	 */
	bool decodeImage(const byte *data, size_t length, Image &image);

	/**
	 * Encode RGBA pixels as a PNG file
	 * @param image image to encode
	 * @return PNG file data, empty for an invalid image
	 */
	std::vector<byte> encodePNG(const Image &image);

	/**
	 * @param data file data
	 * @param length file length
	 * @return "image/png" or "image/jpeg" when the data is a PNG or a JPEG file, else empty
	 */
	std::string imageMimeType(const byte *data, size_t length);
}
