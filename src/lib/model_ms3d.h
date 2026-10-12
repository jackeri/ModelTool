#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load a MilkShape 3D (version 4) model. The model gets the ET default armature information (torso weights and torso
	 * parent), tag bones are marked, duplicate vertices are merged and it is rotated from Y-up to Z-up with the model
	 * transform, like LegacyModelView did.
	 * @param file file to parse
	 * @return loaded skeletal model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMS3D(const Ref<mt::io::MTFile> &file);

	/**
	 * Append the animation of an MS3D file to a skeletal model with the same joints. The frames are added after the
	 * existing frames and an animation sequence named after the file is added.
	 * @param parent model to add the animation to
	 * @param file MS3D file
	 * @throws mt::mt_exception if the data is invalid or the joints do not match
	 */
	void loadMS3DAnimation(Model *parent, const Ref<mt::io::MTFile> &file);
}
