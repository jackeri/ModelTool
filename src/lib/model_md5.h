#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * load and parse an MD5mesh file
	 * @param file file instance to parse
	 * @return loaded Model instance or nullptr if the loading fails
	 */
	Model *loadMd5Model(const Ref<io::MTFile> &file);

	/**
	 * load and parse an animation file
	 * @param parent parent model to which to load the animation information
	 * @param file file instance to parse
	 */
	void loadMD5Anim(Model *parent, const Ref<io::MTFile> &file);
}
