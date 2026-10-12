#pragma once

#include "library.h"
#include "model.h"

namespace mt::model {

	/**
	 * Encode the current pose of a model as STL (all meshes in one solid). The transform is applied. The triangles are
	 * written counter-clockwise with outward facing normals, as STL expects.
	 * @param model model to encode
	 * @param binary binary STL instead of text
	 * @return file data
	 */
	std::vector<byte> encodeSTL(const Model &model, bool binary);

	void writeSTL(const Model &model, const std::string &path, bool binary);
}
