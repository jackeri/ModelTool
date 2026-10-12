#pragma once

#include "library.h"
#include "model.h"
#include "morph_model.h"

namespace mt::model {

	/**
	 * @return matrix of a location and rotation
	 */
	glm::mat4 pointMatrix(const Point &point);

	/**
	 * Location and rotation of a matrix (scale is dropped)
	 */
	Point pointFromMatrix(const glm::mat4 &matrix);

	/**
	 * Location and rotation relative to a parent, for a point moved in the world (like with a gizmo)
	 * @param parentWorld world matrix of the parent (model transform and bone)
	 * @param world new world matrix of the point
	 * @return the point relative to the parent
	 */
	Point localPoint(const glm::mat4 &parentWorld, const glm::mat4 &world);

	/**
	 * Move a morph model tag: the tag in one frame becomes the new point, the other frames move the same way when
	 * allFrames is set (the tag keeps its animation)
	 * @param model morph model
	 * @param tag tag index
	 * @param frame frame that was edited
	 * @param point new location and rotation in that frame (model space)
	 * @param allFrames apply the same change to every frame
	 */
	void moveMorphTag(morph::MorphModel &model, int tag, int frame, const Point &point, bool allFrames);
}
