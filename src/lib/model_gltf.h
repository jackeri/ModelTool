#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load a glTF 2.0 model (.gltf with embedded buffers, or .glb). Skinned scenes become skeletal models with every
	 * animation sampled at 24 frames per second, static scenes become morph models. The scene is converted from Y-up
	 * to Z-up, the coordinates are kept as they are. Nodes named tag_* become tags. External buffer files are not
	 * supported.
	 * @param file file to parse
	 * @return loaded model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadGLTF(const Ref<mt::io::MTFile> &file);
}
