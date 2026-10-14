#include "image.h"

// The stb implementations for the library, the application and the tests
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace mt::io {

	bool decodeImage(const byte *data, size_t length, Image &image)
	{
		int comp = 0;
		byte *pixels = stbi_load_from_memory(data, int(length), &image.width, &image.height, &comp, STBI_rgb_alpha);
		if (!pixels)
		{
			return false;
		}
		image.pixels.assign(pixels, pixels + size_t(image.width) * size_t(image.height) * 4);
		stbi_image_free(pixels);
		return true;
	}

	std::vector<byte> encodePNG(const Image &image)
	{
		std::vector<byte> out;
		if (image.width <= 0 || image.height <= 0 || image.pixels.size() < size_t(image.width) * size_t(image.height) * 4)
		{
			return out;
		}
		stbi_write_png_to_func(
			[](void *context, void *data, int size) {
				auto *target = static_cast<std::vector<byte> *>(context);
				target->insert(target->end(), static_cast<byte *>(data), static_cast<byte *>(data) + size);
			},
			&out, image.width, image.height, 4, image.pixels.data(), image.width * 4);
		return out;
	}

	std::string imageMimeType(const byte *data, size_t length)
	{
		static const byte png[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
		if (length >= 8 && std::equal(png, png + 8, data))
		{
			return "image/png";
		}
		if (length >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
		{
			return "image/jpeg";
		}
		return "";
	}
}
