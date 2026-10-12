#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "morph_model.h"

namespace mt::model {

	/*! Result of the progressive mesh reduction */
	struct ProgressiveMesh {
		std::vector<int> permutation{}; ///< new index of every original vertex
		std::vector<int> collapseMap{}; ///< in the new order: the vertex each vertex collapses into (always a lower index)
		int minLod = 0;					///< vertex count below which only silhouette (border) vertices would be removed
	};

	/**
	 * Stan Melax's polygon reduction ("A Simple, Fast, and Effective Polygon Reduction Algorithm"), as in
	 * LegacyModelView and the game's progressive meshes: vertices are ordered so that the last one is collapsed first
	 * @param positions vertex positions
	 * @param tris triangles
	 * @return vertex permutation, collapse map and minimum LOD vertex count
	 */
	ProgressiveMesh progressiveMesh(const std::vector<glm::vec3> &positions, const std::vector<glm::ivec3> &tris);

	/**
	 * Calculate the collapse maps and minimum LODs of a skeletal model. The vertices of every mesh are reordered.
	 * @param model model to calculate
	 * @param frame pose to measure the mesh in, FRAME_BIND_POSE for the bind pose
	 */
	void calculateLod(skeletal::SkeletalModel &model, int frame);

	/**
	 * Calculate the collapse maps and minimum LODs of the base meshes of a morph model. The vertices are reordered.
	 * @param model model to calculate
	 * @param frame frame to measure the meshes in
	 */
	void calculateLod(morph::MorphModel &model, int frame);

	/**
	 * Calculate the LOD data of any model in its current frame (skeletal models in the bind pose when it is shown)
	 * @param model model to calculate
	 * @return false if the model has nothing to measure
	 */
	bool calculateLod(Model &model);
}
