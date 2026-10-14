#pragma once

#include "library.h"
#include "files.h"
#include "model.h"

#include <map>

namespace mt::model {

	/*! Skin file (.skin): the shader of each mesh (surface), for Quake 3 style and ET models */
	struct Skin {
		std::map<std::string, std::string> surfaces{};	  ///< lowercase surface name -> shader
		std::map<std::string, std::string> accessories{}; ///< ET accessory lines (md3_hat, md3_beltr, ...) -> model

		/**
		 * @param surface mesh name (case insensitive)
		 * @return shader of the surface, empty if the skin does not name it
		 */
		[[nodiscard]] std::string shader(const std::string &surface) const;

		[[nodiscard]] bool empty() const
		{
			return surfaces.empty() && accessories.empty();
		}
	};

	/**
	 * Parse a skin: "surface,shader" lines ("tag_*," lines without a shader are skipped, quotes and // comments are
	 * allowed). Lines starting with md3_ are ET accessories.
	 * @param text skin file text
	 * @return parsed skin
	 */
	Skin parseSkin(const std::string &text);

	/**
	 * Load a skin file
	 * @param file skin file
	 * @return parsed skin
	 * @throws mt::mt_exception if the file is missing
	 */
	Skin loadSkin(const Ref<mt::io::MTFile> &file);

	/**
	 * Text of a file
	 * @param file loaded file
	 * @return file data as text, empty for a missing file
	 */
	std::string fileText(const Ref<mt::io::MTFile> &file);

	/**
	 * Write a skin into a model: the meshes the skin names get its shader as their material, in every level of detail
	 * (like the old Pascal Q3 Model Tool's "Embed skin into model"). Meshes the skin does not name keep their material.
	 * @param model model to change
	 * @param skin skin to write
	 * @return number of meshes whose material changed
	 */
	size_t bakeSkin(Model &model, const Skin &skin);
}
