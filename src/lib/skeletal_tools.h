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

	/**
	 * Make a frame the bind pose (LegacyModelView "Bake pose"): the vertex positions and bind normals are calculated in
	 * the frame, and with armature the joints' bind pose becomes the frame's pose
	 * @param model model to change
	 * @param frame frame to bake
	 * @param armature also move the joints' bind pose to the frame
	 * @throws mt::mt_exception if the frame does not exist
	 */
	void bakeBindPose(SkeletalModel &model, int frame, bool armature = true);

	/**
	 * Can a bone be used as a tag: it has no child bones, no bone tags or hitboxes attached, and no vertex weights
	 * @param model model of the bone
	 * @param index bone index
	 * @return true if the bone can be a tag
	 */
	bool canBeTag(const SkeletalModel &model, int index);

	/**
	 * Turn the bones that are used as tags into bone tags relative to their parent bone (LegacyModelView "Bake tags").
	 * The tag position comes from the bind pose (or the first frame). Bones that can not be tags are left alone.
	 * @param model model to change
	 * @return number of baked tags
	 */
	int bakeTags(SkeletalModel &model);

	/**
	 * Reorder the bones. Every bone index in the model (parents, weights, tags, hitboxes, torso parent) is remapped.
	 * @param model model to change
	 * @param order order[new index] = old index, a permutation of all bones where parents come before children
	 * @throws mt::mt_exception if the order is not a valid permutation or puts a child before its parent
	 */
	void reorderJoints(SkeletalModel &model, const std::vector<int> &order);

	/*! Axes to scale the bounds on */
	enum class BoundsAxes
	{
		X,
		Y,
		Z,
		XY,
		All
	};

	/**
	 * Scale the frame bounds (LegacyModelView "Scale bounding box"), the game uses them for culling
	 * @param model model to change (skeletal or morph)
	 * @param scale scale factor
	 * @param axes axes to scale
	 */
	void scaleBounds(Model &model, float scale, BoundsAxes axes);
}
