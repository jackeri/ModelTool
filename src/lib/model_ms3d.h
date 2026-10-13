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

	/**
	 * Encode a skeletal model as MilkShape 3D (version 4, Y-up). The transform and the export frames are applied. The
	 * bone tags become tag bones after the bones (MS3D has no tags, the loader turns tag_* bones back into tags), every
	 * frame is a key frame, up to 4 bone weights per vertex, a material per mesh with its texture name. A model without
	 * a bind pose gets its first frame as the bind pose.
	 * @param model skeletal model to encode
	 * @return file data
	 * @throws mt::mt_exception for morph models or a model that does not fit (65535 vertices / triangles, 127 bones,
	 * 31 character names)
	 */
	std::vector<byte> encodeMS3D(const Model &model);

	/**
	 * Write a skeletal model as MilkShape 3D
	 */
	void writeMS3D(const Model &model, const std::string &path);
}
