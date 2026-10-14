#pragma once

#include "library.h"
#include "model.h"

namespace mt::model {

	/*! The default weld distance, like the old Pascal Q3 Model Tool's "Fix cracks & gaps" */
	constexpr float WELD_DISTANCE = 0.15f;

	/*! What a weld did */
	struct WeldResult {
		size_t groups = 0;	   ///< groups of two or more vertices that were joined
		size_t moved = 0;	   ///< vertices that changed place
		size_t reweighted = 0; ///< skeletal models: vertices that took other weights (a seam that tore apart in animation)
	};

	/**
	 * Close the cracks and gaps between vertices that are almost at the same place: vertices closer than the distance
	 * are moved to one place, in every frame. The vertices stay separate (they keep their texture coordinates and
	 * normals, recalculate the normals afterwards if needed).
	 *
	 * The groups are found in one pose (the bind pose of skeletal models with one, else the current frame), so they stay
	 * together in every frame. Morph models: each group moves to its average place in every frame, every level of detail
	 * on its own. Skeletal models: each vertex of a group gets the weights of the group's first vertex, so they share
	 * their place in every pose.
	 * @param model model to change
	 * @param distance largest distance between the vertices that are welded
	 * @param acrossMeshes also weld vertices of different meshes (like the seam between two body parts)
	 * @return what was welded
	 */
	WeldResult weldVertices(Model &model, float distance = WELD_DISTANCE, bool acrossMeshes = false);
}
