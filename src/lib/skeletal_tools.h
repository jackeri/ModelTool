#pragma once

#include "library.h"
#include "skeletal_model.h"

namespace mt::model::skeletal {

	/**
	 * Position of a vertex in a pose
	 * @param model model that owns the joints
	 * @param mesh mesh of the vertex
	 * @param vert the vertex
	 * @param frame pose, FRAME_BIND_POSE for the bind pose
	 * @return object space position, the bind pose position for vertices without weights
	 */
	glm::vec3 posedVertex(const SkeletalModel &model, const Mesh &mesh, const SkeletalVertex &vert, int frame);

	/**
	 * Calculate the frame bounds from the posed vertices
	 * @param model model to calculate the bounds for, model.bounds is resized to the frame count
	 * @param scale scale applied to the bounds (LegacyModelView used 1.2 for MS3D models)
	 * @param startFrame first frame to calculate
	 * @param endFrame one past the last frame to calculate, -1 for all frames
	 */
	void calculateBounds(SkeletalModel &model, float scale = 1.0f, int startFrame = 0, int endFrame = -1);

	/**
	 * Merge vertices that have the same position, texture coordinates, normals and weights. The triangles are remapped
	 * and unused weights are removed. The collapse maps of the changed meshes are cleared (they no longer match).
	 * @param model model to optimize
	 * @return number of removed vertices
	 */
	size_t optimizeVertices(SkeletalModel &model);
}
