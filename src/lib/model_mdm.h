#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load an MDM (ET skeletal mesh). The model has no bones or bind pose until an MDX animation is loaded into it.
	 * @param file file to parse
	 * @return loaded skeletal model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMDM(const Ref<mt::io::MTFile> &file);

	/**
	 * Load an MDS (RtCW skeletal model, mesh, bones and animation in one file)
	 * @param file file to parse
	 * @return loaded skeletal model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMDS(const Ref<mt::io::MTFile> &file);

	/**
	 * Encode a skeletal model as MDM (the mesh, the bones come from the MDX). The transform is applied. Bones used as
	 * tags become MDM tags. Meshes without collapse maps get their level of detail calculated.
	 * @param model model to encode
	 * @return file data
	 * @throws mt::mt_exception if the model can not be written as MDM
	 */
	std::vector<byte> encodeMDM(const Model &model);

	/**
	 * Write a skeletal model as an MDM file
	 */
	void writeMDM(const Model &model, const std::string &path);

	/**
	 * Encode a skeletal model as MDS (mesh, bones and frames in one file). The transform and the export frames are
	 * applied. Tags become tag bones.
	 * @param model model to encode
	 * @return file data
	 * @throws mt::mt_exception if the model can not be written as MDS
	 */
	std::vector<byte> encodeMDS(const Model &model);

	/**
	 * Write a skeletal model as an MDS file
	 */
	void writeMDS(const Model &model, const std::string &path);
}
