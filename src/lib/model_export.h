#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "morph_model.h"

namespace mt::model {

	/**
	 * Copy a skeletal or morph model
	 * @param model model to copy
	 * @return copy, owned by the caller
	 */
	std::unique_ptr<Model> cloneModel(const Model &model);

	/**
	 * Apply the model's transform (location, rotation and scale) to its data and reset the transform
	 * @param model model to change
	 */
	void bakeTransform(Model &model);

	/**
	 * Keep only a range of frames. Animation sequences are moved and clipped to the range, sequences outside of it are
	 * removed.
	 * @param model model to change
	 * @param first first frame to keep
	 * @param last last frame to keep
	 */
	void cropFrames(Model &model, int first, int last);

	/**
	 * Copy of the model ready for writing: the transform is applied and the frames are cropped to the export frames
	 * @param model model to export
	 * @return prepared copy
	 */
	std::unique_ptr<Model> prepareForExport(const Model &model);

	/**
	 * Bake a skeletal model into a morph model: every frame (or the bind pose when there are no frames) becomes a
	 * morph frame, bone tags and bones used as tags become morph tags
	 * @param model skeletal model
	 * @return morph model, owned by the caller
	 * @throws mt::mt_exception if the model has nothing to show (no frames and no bind pose)
	 */
	std::unique_ptr<morph::MorphModel> toMorphModel(const skeletal::SkeletalModel &model);

	/**
	 * Bones that the ET formats write as tags instead of bones: used as tags and nothing depends on them
	 * @param model skeletal model
	 * @return flag per bone
	 */
	std::vector<bool> tagBones(const skeletal::SkeletalModel &model);

	/**
	 * Bone tag (relative to the parent bone) of a bone that is used as a tag. The pose comes from the bind pose, or the
	 * first frame when there is no bind pose. The flipBoneTags rotation is applied.
	 * @param model skeletal model
	 * @param index bone index, needs a parent
	 * @return tag
	 */
	skeletal::BoneTag boneTagFromJoint(const skeletal::SkeletalModel &model, int index);
}
