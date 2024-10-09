#include "skeletal_model.h"

namespace mt::model::skeletal {

	SkeletalModel::~SkeletalModel()
	{
		emptyVector(joints);
		emptyVector(meshes);
		emptyVector(tags);
		emptyVector(bounds);
	}

	int SkeletalModel::numFrames()
	{
		return int(joints.empty() ? 0 : joints[0].frames.size());
	}

	int SkeletalModel::findParentIndex(int boneIndex)
	{
		if (boneIndex == -1)
		{
			return -1;
		}

		int foundParent = 0;
		for (auto i = 0; i < boneIndex; i++)
		{
			if (!joints[i].asTag)
			{
				foundParent++;
			}
		}

		return foundParent;
	}

	bool SkeletalModel::hasTags()
	{
		if (!tags.empty())
		{
			return true;
		}

		return std::any_of(joints.begin(), joints.end(), [](const Joint &joint) { return joint.asTag; });
	}
}
