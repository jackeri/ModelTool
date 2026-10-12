#include "texture_lookup.h"

#include <filesystem>

namespace mt::io {

	const std::vector<std::string> &textureExtensions()
	{
		static const std::vector<std::string> extensions = {".tga", ".jpg", ".jpeg", ".png", ".bmp", ".psd", ".gif", ".hdr", ".pic"};
		return extensions;
	}

	static bool isImageExtension(const std::string &ext)
	{
		std::string lower = ext;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		const auto &extensions = textureExtensions();
		return std::find(extensions.begin(), extensions.end(), lower) != extensions.end();
	}

	std::vector<std::string> textureCandidates(const std::string &material, const std::string &modelPath)
	{
		std::string name = material;
		std::replace(name.begin(), name.end(), '\\', '/');

		// Drive letters of absolute Windows paths ("C:/mapping/etmain/models/...")
		if (name.size() > 1 && name[1] == ':')
		{
			name.erase(0, 2);
		}
		while (!name.empty() && name[0] == '/')
		{
			name.erase(0, 1);
		}

		std::vector<std::string> out;
		if (name.empty())
		{
			return out;
		}

		auto add = [&](const std::string &path) {
			if (!path.empty() && std::find(out.begin(), out.end(), path) == out.end())
			{
				out.push_back(path);
			}
		};

		// The name, then shorter versions without the leading folders (absolute paths from the artist's machine)
		std::vector<std::string> names{name};
		for (size_t slash = name.find('/'); slash != std::string::npos; slash = name.find('/', slash + 1))
		{
			names.push_back(name.substr(slash + 1));
		}

		std::string folder = std::filesystem::path(modelPath).parent_path().generic_string();

		std::vector<std::string> bases;
		for (const auto &candidate : names)
		{
			bases.push_back(candidate);
		}
		if (!folder.empty())
		{
			// Next to the model only the file name is used
			bases.push_back(folder + "/" + names.back());
			if (names.size() > 1)
			{
				bases.push_back(folder + "/" + names.front());
			}
		}

		for (const auto &base : bases)
		{
			add(base);

			// Strip every image extension ("Head.tga.png" -> "Head") and try the others
			std::filesystem::path path(base);
			while (path.has_extension() && isImageExtension(path.extension().string()))
			{
				path = path.parent_path() / path.stem();
			}

			std::string stem = path.generic_string();
			for (const auto &ext : textureExtensions())
			{
				add(stem + ext);
			}
		}

		return out;
	}

	std::string findTexture(FileSource &source, const std::string &material, const std::string &modelPath)
	{
		for (const auto &candidate : textureCandidates(material, modelPath))
		{
			if (source.findFile(candidate))
			{
				return candidate;
			}
		}
		return "";
	}

	Image applyPicmip(const Image &image, int picmip, int minSize)
	{
		Image out = image;
		for (int i = 0; i < picmip && out.width / 2 >= minSize && out.height / 2 >= minSize; i++)
		{
			Image half;
			half.width = out.width / 2;
			half.height = out.height / 2;
			half.pixels.resize(size_t(half.width) * size_t(half.height) * 4);

			for (int y = 0; y < half.height; y++)
			{
				for (int x = 0; x < half.width; x++)
				{
					for (int c = 0; c < 4; c++)
					{
						auto at = [&](int px, int py) {
							return int(out.pixels[(size_t(py) * size_t(out.width) + size_t(px)) * 4 + size_t(c)]);
						};
						int sum = at(x * 2, y * 2) + at(x * 2 + 1, y * 2) + at(x * 2, y * 2 + 1) + at(x * 2 + 1, y * 2 + 1);
						half.pixels[(size_t(y) * size_t(half.width) + size_t(x)) * 4 + size_t(c)] = byte((sum + 2) / 4);
					}
				}
			}

			out = std::move(half);
		}
		return out;
	}
}
