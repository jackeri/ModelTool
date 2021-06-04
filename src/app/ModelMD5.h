#pragma once

#include "mt.h"
#include "Model.h"
#include "filesystem.h"

namespace mt::model {

	/**
	 * load and parse an MD5mesh file
	 * @param file file instance to parse
	 * @return loaded Model instance or nullptr if the loading fails
	 */
	Model *loadMd5Model(const Ref<IO::MTFile> &file);

	/**
	 * load and parse an animation file
	 * @param parent parent model to which to load the animation information
	 * @param file file instance to parse
	 */
	void loadMD5Anim(Model *parent, const Ref<IO::MTFile> &file);
}
