#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "files.h"

namespace mt::io {
	class BinaryReader;
}

namespace mt::idtech {
	struct skelBoneInfo_t;
}

namespace mt::model {

	/**
	 * Load an MDX file as a model (skeleton and animation frames, no meshes)
	 * @param file file to parse
	 * @return loaded skeletal model, owned by the caller
	 * @throws mt::mt_exception if the data is invalid
	 */
	Model *loadMDX(const Ref<mt::io::MTFile> &file);

	/**
	 * Load an MDX animation into a skeletal model (usually an MDM). Creates the bones if the model has none, otherwise
	 * the bones need to match. Replaces the existing frames.
	 * @param parent model to load the animation into
	 * @param file file to parse
	 * @throws mt::mt_exception if the data is invalid or does not match the model
	 */
	void loadMDXAnimation(Model *parent, const Ref<mt::io::MTFile> &file);

	namespace detail {
		/**
		 * Create the joints of a model from MDS/MDX bone infos
		 * @param model model to add the joints to
		 * @param bones bone infos from the file
		 */
		void createJoints(skeletal::SkeletalModel &model, const std::vector<idtech::skelBoneInfo_t> &bones);

		/**
		 * Decode the compressed MDS/MDX frames into joint frames and model bounds
		 * @param reader reader positioned anywhere, offsets are absolute
		 * @param ofsFrames offset of the first frame
		 * @param numFrames number of frames
		 * @param bones bone infos from the file
		 * @param model model to add the frames to, the joints need to exist
		 */
		void readBoneFrames(const mt::io::BinaryReader &reader, int64_t ofsFrames, int32_t numFrames,
							const std::vector<idtech::skelBoneInfo_t> &bones, skeletal::SkeletalModel &model);

		/**
		 * Check that every weight in the model points to an existing joint
		 * @param model model to check
		 * @throws mt::mt_exception on an invalid joint index
		 */
		void validateWeights(const skeletal::SkeletalModel &model);
	}
}
