#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "morph_model.h"

namespace mt::model {

	/*! How vertex normals are calculated */
	enum class NormalMode
	{
		Smooth, ///< average of the normals of every triangle using the vertex
		Flat,	///< normal of a single triangle using the vertex (vertices are not split)
	};

	/**
	 * Calculate the normals of a skeletal mesh in a pose. Sets the joint-local vertex normals used for animation, and the
	 * bind pose normals if the pose is the bind pose.
	 * @param model model that owns the joints (the mesh does not need to be in the model yet)
	 * @param mesh mesh to calculate the normals for
	 * @param frame pose to use, FRAME_BIND_POSE for the bind pose
	 * @param mode normal calculation mode
	 */
	void calculateNormals(const skeletal::SkeletalModel &model, skeletal::Mesh &mesh, int frame = FRAME_BIND_POSE,
						  NormalMode mode = NormalMode::Smooth);

	/**
	 * Set the normals of a skeletal mesh from known normals (like the normals stored in a file). Sets the joint-local
	 * vertex normals used for animation, and the bind pose normals if the pose is the bind pose.
	 * @param model model that owns the joints (the mesh does not need to be in the model yet)
	 * @param mesh mesh to set the normals for
	 * @param normals normal per vertex in the pose
	 * @param frame pose the normals are in, FRAME_BIND_POSE for the bind pose
	 */
	void setNormals(const skeletal::SkeletalModel &model, skeletal::Mesh &mesh, const std::vector<glm::vec3> &normals,
					int frame = FRAME_BIND_POSE);

	/**
	 * Calculate the normals of every mesh in a skeletal model. Uses the bind pose if the model has one, otherwise the
	 * current frame.
	 * @param model model to calculate the normals for
	 * @param mode normal calculation mode
	 * @return false if the model has no bind pose and no frames, so there is nothing to calculate from
	 */
	bool calculateNormals(skeletal::SkeletalModel &model, NormalMode mode = NormalMode::Smooth);

	/**
	 * Calculate the normals of every frame of every level of detail in a morph model
	 * @param model model to calculate the normals for
	 * @param mode normal calculation mode
	 */
	void calculateNormals(morph::MorphModel &model, NormalMode mode = NormalMode::Smooth);

	/**
	 * Calculate the normals of any supported model type
	 * @param model model to calculate the normals for
	 * @param mode normal calculation mode
	 * @return false if the normals could not be calculated
	 */
	bool calculateNormals(Model &model, NormalMode mode = NormalMode::Smooth);
}
