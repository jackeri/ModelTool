#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "files.h"

namespace mt::model {

	/**
	 * Load animation sequences from an .aninc file into a model, replacing the existing sequences.
	 * Each line is: name first_frame length looping fps move_speed transition reversed
	 * @param model model to load the sequences into
	 * @param file file to parse
	 * @throws mt::mt_exception if the data is invalid
	 */
	void loadAninc(Model *model, const Ref<mt::io::MTFile> &file);

	/**
	 * Load bone tags from a tags file (LegacyModelView format) and add them to the model.
	 * Format: tags { "name" "parent bone name" ( x y z ) ( qx qy qz qw ) ... }
	 * @param model model to add the tags to, the parent bones need to exist
	 * @param file file to parse
	 * @throws mt::mt_exception if the data is invalid or a parent bone is not found
	 */
	void loadTags(skeletal::SkeletalModel &model, const Ref<mt::io::MTFile> &file);
}
