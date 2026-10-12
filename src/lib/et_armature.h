#pragma once

#include "library.h"
#include "skeletal_model.h"

namespace mt::model::et {
	using skeletal::SkeletalModel;

	/*! Bone of the default ET player armature */
	struct DefaultBone {
		const char *name;  ///< bone name
		float torsoWeight; ///< default torso weight
		int parentId;	   ///< parent bone in the default armature
	};

	/**
	 * @return the default ET player armature (Biped from 3ds Max, 53 bones)
	 */
	const std::vector<DefaultBone> &defaultArmature();

	/**
	 * Index of the torso parent bone in the default armature
	 */
	const int DEFAULT_TORSO_PARENT = 1;

	/**
	 * Set the torso weights and the torso parent of bones that are found in the default armature (by name, case
	 * insensitive). If the torso parent bone is not found, the first root bone is used.
	 * @param model model to update
	 */
	void applyDefaultJointInformation(SkeletalModel &model);

	/**
	 * Mark bones without children (or tags) that are named "tag_*" as tags
	 * @param model model to update
	 */
	void markTagBones(SkeletalModel &model);

	/**
	 * Compare the armature with the default ET armature
	 * @param model model to check
	 * @return human readable list of differences, empty if the armature matches
	 */
	std::vector<std::string> checkArmature(const SkeletalModel &model);
}
