#pragma once

#include "library.h"
#include "model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load an IQM (Inter-Quake Model, version 2) model with its skeleton, animation frames and animation sequences.
	 * Joint scaling is not supported and ignored.
	 * @param file file to parse
	 * @return loaded skeletal model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadIQM(const Ref<mt::io::MTFile> &file);
}
