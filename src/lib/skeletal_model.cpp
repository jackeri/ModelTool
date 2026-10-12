#include "skeletal_model.h"

namespace mt::model::skeletal {

	SkeletalModel::~SkeletalModel()
	{
		emptyVector(joints);
		emptyVector(meshes);
		emptyVector(tags);
		emptyVector(bounds);
	}

	int SkeletalModel::numFrames() const
	{
		return int(joints.empty() ? 0 : joints[0].frames.size());
	}

	bool SkeletalModel::hasTags() const
	{
		if (!tags.empty())
		{
			return true;
		}

		return std::any_of(joints.begin(), joints.end(), [](const Joint &joint) { return joint.asTag; });
	}
}
