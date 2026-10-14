#include "skin.h"
#include "morph_model.h"
#include "skeletal_model.h"

namespace mt::model {

	namespace {
		std::string lowercase(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}

		std::string trim(const std::string &text)
		{
			const char *space = " \t\r\n\"";
			auto first = text.find_first_not_of(space);
			if (first == std::string::npos)
			{
				return {};
			}
			auto last = text.find_last_not_of(space);
			return text.substr(first, last - first + 1);
		}
	}

	std::string Skin::shader(const std::string &surface) const
	{
		auto found = surfaces.find(lowercase(surface));
		return found == surfaces.end() ? std::string() : found->second;
	}

	Skin parseSkin(const std::string &text)
	{
		Skin skin;
		std::istringstream lines(text);
		for (std::string line; std::getline(lines, line);)
		{
			if (auto comment = line.find("//"); comment != std::string::npos)
			{
				line.erase(comment);
			}

			// "surface,shader", ET skins can also separate them with spaces only
			auto split = line.find(',');
			if (split == std::string::npos)
			{
				std::string trimmed = trim(line);
				split = trimmed.find_first_of(" \t");
				line = trimmed;
			}
			if (split == std::string::npos)
			{
				continue;
			}

			std::string name = lowercase(trim(line.substr(0, split)));
			std::string value = trim(line.substr(split + 1));
			if (name.empty() || value.empty())
			{
				continue;
			}

			if (tools::startsWith(name, "md3_"))
			{
				skin.accessories[name] = value;
			}
			else
			{
				skin.surfaces[name] = value;
			}
		}
		return skin;
	}

	std::string fileText(const Ref<mt::io::MTFile> &file)
	{
		if (!file || !file->data)
		{
			return {};
		}
		// The buffers can have a terminating zero after the data
		size_t size = std::min(file->len ? file->len : file->data->size(), file->data->size());
		std::string text(reinterpret_cast<const char *>(file->data->data()), size);
		if (auto zero = text.find('\0'); zero != std::string::npos)
		{
			text.resize(zero);
		}
		return text;
	}

	Skin loadSkin(const Ref<mt::io::MTFile> &file)
	{
		if (!file || !file->data)
		{
			throw mt_ex("Skin file not found");
		}
		return parseSkin(fileText(file));
	}

	size_t bakeSkin(Model &model, const Skin &skin)
	{
		size_t changed = 0;
		auto bake = [&](const std::string &name, std::string &material) {
			const std::string shader = skin.shader(name);
			if (!shader.empty() && shader != material)
			{
				material = shader;
				changed++;
			}
		};

		if (auto *skeletal = dynamic_cast<skeletal::SkeletalModel *>(&model))
		{
			for (auto &mesh : skeletal->meshes)
			{
				bake(mesh.name, mesh.materialName);
			}
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			for (auto &mesh : morph->meshes)
			{
				bake(mesh.name, mesh.materialName);
			}
			for (auto &level : morph->lods)
			{
				for (auto &mesh : level)
				{
					bake(mesh.name, mesh.materialName);
				}
			}
		}
		return changed;
	}
}
