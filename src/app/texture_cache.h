#pragma once

#include "mt.h"
#include "texture.h"
#include "../lib/files.h"
#include "../lib/shader_script.h"

#include <map>

namespace mt {

	/*! Loads and keeps the textures of materials. Missing textures are remembered so they are only looked up once. */
	class TextureCache {
	  public:
		/**
		 * Get the texture of a material, loading it on the first call. Like the game, the image of the shader with the
		 * material's name is used when there is one (see loadShaders), else the image named like the material
		 * @param source file source to find the texture from
		 * @param material material name of the mesh
		 * @param modelPath path of the model, textures are also looked up next to it
		 * @param images images carried in the model (embedded in a glTF file) by material, they come first
		 * @return texture or nullptr if the texture was not found or could not be decoded
		 */
		Texture *get(io::FileSource &source, const std::string &material, const std::string &modelPath,
					 const std::map<std::string, std::vector<byte>> *images = nullptr);

		/**
		 * Change the picmip and filtering, the textures are reloaded when they are used next
		 * @param picmip number of times the textures are halved
		 * @param filter texture filtering
		 */
		void setOptions(int picmip, TextureFilter filter);

		/**
		 * A side of a sky box: kept apart from the model textures, so loading or closing a model does not reload it
		 * (only new base folders or another texture filter do); full size and clamped to the edges like in the game
		 * @param source file source to find the image from
		 * @param image image name (sky box name + _rt, _bk, ...)
		 * @return texture or nullptr if the image was not found
		 */
		Texture *skySide(io::FileSource &source, const std::string &image);

		/**
		 * Forget every model texture (for example when a new model is loaded), the shaders and the sky box are kept
		 */
		void clear();

		/**
		 * Read the shader scripts (scripts/<name>.shader) of the base folders, and forget the textures
		 * @param files file system of the base folders
		 */
		void loadShaders(io::FileSystem &files);

		/**
		 * @param material material name
		 * @return the shader of the material, nullptr if it has none
		 */
		[[nodiscard]] const model::ShaderDef *shader(const std::string &material) const
		{
			return shaders.find(material);
		}

		/**
		 * @return the loaded shaders
		 */
		[[nodiscard]] const model::ShaderLibrary &shaderLibrary() const
		{
			return shaders;
		}

	  private:
		std::map<std::string, std::unique_ptr<Texture>> textures{}; ///< key is the model path + material
		/*! Load a texture through the shaders and the files */
		std::unique_ptr<Texture> loadFile(io::FileSource &source, const std::string &material, const std::string &modelPath, bool sky);

		std::map<std::string, std::unique_ptr<Texture>> skyTextures{}; ///< sky box sides by image name
		model::ShaderLibrary shaders{};
		int picmip = 0;
		TextureFilter filter = TextureFilter::Trilinear;
	};
}
