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

	/**
	 * Encode a level of detail of a model as MD3. The transform and the export frames are applied, skeletal models are
	 * baked into morph frames first.
	 * @param model model to encode
	 * @param level level of detail (0 for the base meshes)
	 * @param tagsOnly write only the tags (MD3 tag animation, no surfaces)
	 * @return file data
	 * @throws mt::mt_exception if the model does not fit in the MD3 limits
	 */
	std::vector<byte> encodeMD3(const Model &model, int level = 0, bool tagsOnly = false);

	/**
	 * Write a model as MD3, extra levels of detail go to path_1.md3, path_2.md3, ...
	 */
	void writeMD3(const Model &model, const std::string &path);

	/**
	 * Write only the tags of a model as an MD3 (tag animation)
	 */
	void writeMD3Tags(const Model &model, const std::string &path);
}
