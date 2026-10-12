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
}
