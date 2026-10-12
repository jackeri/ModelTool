#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load an FBX model (binary or ASCII) with ufbx. Skinned scenes become skeletal models with every animation stack
	 * sampled into frames, static scenes become morph models. The scene is converted to Z-up, the coordinates are kept
	 * as they are (no unit conversion). Nodes named tag_* become tags.
	 * @param file file to parse
	 * @return loaded model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadFBX(const Ref<mt::io::MTFile> &file);

	/**
	 * Load a Wavefront OBJ model with ufbx as a static (morph) model. OBJ files have no units or axes, the coordinates
	 * are used as they are. Materials from .mtl files are not loaded, the material names are kept.
	 * @param file file to parse
	 * @return loaded model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadOBJ(const Ref<mt::io::MTFile> &file);
}
