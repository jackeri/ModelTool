#pragma once

#include "library.h"
#include "model.h"
#include "morph_model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load an MD3 model
	 * @param file file to parse
	 * @return loaded morph model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMD3(const Ref<mt::io::MTFile> &file);

	/**
	 * Load an MDC (compressed MD3) model
	 * @param file file to parse
	 * @return loaded morph model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMDC(const Ref<mt::io::MTFile> &file);

	/**
	 * Add a level of detail to a morph model from an MD3 or MDC file (like model_1.md3). The level needs to have the
	 * same number of frames as the model, its frames and tags are not used.
	 * @param model model to add the level of detail to
	 * @param file MD3 or MDC file
	 * @throws mt::mt_exception if the data is invalid or does not match the model
	 */
	void addMorphLod(morph::MorphModel &model, const Ref<mt::io::MTFile> &file);

	/**
	 * Name of a level of detail file in the game's naming scheme (models/head.md3 -> models/head_1.md3)
	 * @param path path of the base model
	 * @param level level of detail, 1 and up
	 * @return path of the level of detail file
	 */
	std::string morphLodPath(const std::string &path, int level);
}
