#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * load and parse an MD5mesh file
	 * @param file file instance to parse
	 * @return loaded Model instance, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMd5Model(const Ref<mt::io::MTFile> &file);

	/**
	 * load and parse an animation file
	 * @param parent parent model to which to load the animation information
	 * @param file file instance to parse
	 */
	void loadMD5Anim(Model *parent, const Ref<mt::io::MTFile> &file);

	/**
	 * Encode a skeletal model as an MD5 mesh. The transform is applied. Without a bind pose the first export frame is
	 * used as the bind pose.
	 * @param model model to encode
	 * @return file text
	 * @throws mt::mt_exception if the model is not skeletal or has nothing to use as the bind pose
	 */
	std::string encodeMD5Mesh(const Model &model);

	/**
	 * Encode the frames of a skeletal model as an MD5 animation. The transform and the export frames are applied.
	 * @param model model to encode
	 * @return file text
	 * @throws mt::mt_exception if the model is not skeletal or has no frames
	 */
	std::string encodeMD5Anim(const Model &model);

	void writeMD5Mesh(const Model &model, const std::string &path);
	void writeMD5Anim(const Model &model, const std::string &path);
}
