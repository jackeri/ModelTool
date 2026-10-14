#include "texture_cache.h"

namespace mt {

	Texture *TextureCache::get(io::FileSource &source, const std::string &material, const std::string &modelPath)
	{
		if (material.empty())
		{
			return nullptr;
		}

		const std::string key = modelPath + "|" + material;
		auto found = textures.find(key);
		if (found != textures.end())
		{
			return found->second.get();
		}

		std::unique_ptr<Texture> texture;
		const std::string path = model::findMaterialTexture(source, shaders, material, modelPath);
		const model::ShaderDef *materialShader = shaders.find(material);
		auto logger = spdlog::get(MT_LOGGER);

		if (path.empty())
		{
			if (logger)
			{
				logger->warn("Texture not found for material: {}", material);
			}
		}
		else if (auto file = source.loadFile(path); file && file->valid())
		{
			io::Image image;
			const size_t length = file->len ? std::min(file->len, file->data->size()) : file->data->size();
			if (Texture::decode(file->data->data(), length, image))
			{
				texture = std::make_unique<Texture>(path);
				// nopicmip shaders keep their full size, like in the game
				texture->upload(io::applyPicmip(image, materialShader && materialShader->noPicmip ? 0 : picmip), filter);
				if (materialShader && materialShader->clamp)
				{
					texture->setClamp(true);
				}
				if (logger)
				{
					logger->info("Loaded texture {} for {}", path, material);
				}
			}
			else if (logger)
			{
				logger->warn("Could not decode texture: {}", path);
			}
		}

		Texture *result = texture.get();
		textures[key] = std::move(texture);
		return result;
	}

	void TextureCache::setOptions(int newPicmip, TextureFilter newFilter)
	{
		if (newPicmip == picmip && newFilter == filter)
		{
			return;
		}

		picmip = newPicmip;
		filter = newFilter;
		clear();
	}

	void TextureCache::clear()
	{
		textures.clear();
	}

	void TextureCache::loadShaders(io::FileSystem &files)
	{
		clear();
		const size_t scripts = shaders.load(files);
		if (auto logger = spdlog::get(MT_LOGGER); logger && scripts > 0)
		{
			logger->info("Loaded {} shaders from {} shader scripts", shaders.size(), scripts);
		}
	}
}
