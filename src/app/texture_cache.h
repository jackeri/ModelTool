#pragma once

#include "mt.h"
#include "texture.h"
#include "../lib/files.h"

#include <map>

namespace mt {

	/*! Loads and keeps the textures of materials. Missing textures are remembered so they are only looked up once. */
	class TextureCache {
	  public:
		/**
		 * Get the texture of a material, loading it on the first call
		 * @param source file source to find the texture from
		 * @param material material name of the mesh
		 * @param modelPath path of the model, textures are also looked up next to it
		 * @return texture or nullptr if the texture was not found or could not be decoded
		 */
		Texture *get(io::FileSource &source, const std::string &material, const std::string &modelPath);

		/**
		 * Change the picmip and filtering, the textures are reloaded when they are used next
		 * @param picmip number of times the textures are halved
		 * @param filter texture filtering
		 */
		void setOptions(int picmip, TextureFilter filter);

		/**
		 * Forget every texture (for example when a new model is loaded)
		 */
		void clear();

	  private:
		std::map<std::string, std::unique_ptr<Texture>> textures{}; ///< key is the model path + material
		int picmip = 0;
		TextureFilter filter = TextureFilter::Trilinear;
	};
}
