#pragma once

#include "library.h"
#include "model.h"
#include "skeletal_model.h"
#include "files.h"

namespace mt::io {
	class BinaryReader;
	class BinaryWriter;
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

	/**
	 * Encode a skeletal model's animation as MDX. The transform and the export frames are applied. Bones used as tags
	 * are left out (they are MDM tags). Without frames the bind pose is written as a single frame.
	 * @param model model to encode
	 * @return file data
	 * @throws mt::mt_exception if the model can not be written as MDX (no bones, several root bones, long names)
	 */
	std::vector<byte> encodeMDX(const Model &model);

	/**
	 * Write a skeletal model's animation as an MDX file
	 * @param model model to write
	 * @param path local path
	 */
	void writeMDX(const Model &model, const std::string &path);

	namespace detail {
		/*! Bones written to an ET skeletal file */
		struct EtSkeleton {
			std::vector<int> bones{}; ///< written bones, export index -> model bone index
			std::vector<int> remap{}; ///< model bone index -> export index, -1 if not written
		};

		/**
		 * Bones to write
		 * @param model skeletal model
		 * @param tagBonesAsTags leave out the bones that are used as tags (MDM/MDX); MDS writes every bone
		 * @return written bones
		 * @throws mt::mt_exception if the model has no bones or several root bones
		 */
		EtSkeleton etSkeleton(const skeletal::SkeletalModel &model, bool tagBonesAsTags);

		/**
		 * Write the compressed frames (frame header + bones) of MDS / MDX
		 * @param out writer
		 * @param model skeletal model, the bounds are used for every frame when they exist
		 * @param skeleton written bones
		 */
		void writeBoneFrames(mt::io::BinaryWriter &out, const skeletal::SkeletalModel &model, const EtSkeleton &skeleton);

		/**
		 * Write the bone infos of MDS / MDX
		 */
		void writeBoneInfos(mt::io::BinaryWriter &out, const skeletal::SkeletalModel &model, const EtSkeleton &skeleton);

		/**
		 * Number of frames written (the bind pose is one frame when there are no frames)
		 */
		int etFrameCount(const skeletal::SkeletalModel &model);

		/**
		 * Pose of a bone in a written frame
		 */
		const Point &etFramePose(const skeletal::SkeletalModel &model, int bone, int frame);

		/**
		 * Make sure every written frame has bounds (calculated from the vertices or the bones when missing)
		 */
		void ensureBounds(skeletal::SkeletalModel &model);

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
