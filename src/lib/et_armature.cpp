#include "et_armature.h"
#include "skeletal_tools.h"

namespace mt::model::et {
	using skeletal::BoneTag;
	using skeletal::Joint;

	const std::vector<DefaultBone> &defaultArmature()
	{
		static const std::vector<DefaultBone> bones = {
			{"Bip01 Pelvis", 0.f, -1},	   {"Bip01 Spine", 0.f, 0},		  {"Bip01 Spine1", 0.3f, 1},	 {"Bip01 Spine2", 0.55f, 2},
			{"Bip01 Spine3", 0.85f, 3},	   {"Bip01 Neck", 1.f, 4},		  {"Bip01 Head", 1.f, 5},		 {"Bip01 L Clavicle", 1.f, 5},
			{"Bip01 L UpperArm", 1.f, 7},  {"Bip01 L Forearm", 1.f, 8},	  {"Bip01 L Hand", 1.f, 9},		 {"Bip01 L Finger0", 1.f, 10},
			{"Bip01 L Finger01", 1.f, 11}, {"Bip01 L Finger02", 1.f, 12}, {"Bip01 L Finger1", 1.f, 10},	 {"Bip01 L Finger11", 1.f, 14},
			{"Bip01 L Finger12", 1.f, 15}, {"Bip01 L Finger2", 1.f, 10},  {"Bip01 L Finger21", 1.f, 17}, {"Bip01 L Finger22", 1.f, 18},
			{"Bip01 L Finger3", 1.f, 10},  {"Bip01 L Finger31", 1.f, 20}, {"Bip01 L Finger32", 1.f, 21}, {"Bip01 L Finger4", 1.f, 10},
			{"Bip01 L Finger41", 1.f, 23}, {"Bip01 L Finger42", 1.f, 24}, {"Bip01 R Clavicle", 1.f, 5},	 {"Bip01 R UpperArm", 1.f, 26},
			{"Bip01 R Forearm", 1.f, 27},  {"Bip01 R Hand", 1.f, 28},	  {"Bip01 R Finger0", 1.f, 29},	 {"Bip01 R Finger01", 1.f, 30},
			{"Bip01 R Finger02", 1.f, 31}, {"Bip01 R Finger1", 1.f, 29},  {"Bip01 R Finger11", 1.f, 33}, {"Bip01 R Finger12", 1.f, 34},
			{"Bip01 R Finger2", 1.f, 29},  {"Bip01 R Finger21", 1.f, 36}, {"Bip01 R Finger22", 1.f, 37}, {"Bip01 R Finger3", 1.f, 29},
			{"Bip01 R Finger31", 1.f, 39}, {"Bip01 R Finger32", 1.f, 40}, {"Bip01 R Finger4", 1.f, 29},	 {"Bip01 R Finger41", 1.f, 42},
			{"Bip01 R Finger42", 1.f, 43}, {"Bip01 L Thigh", 0.f, 1},	  {"Bip01 L Calf", 0.f, 45},	 {"Bip01 L Foot", 0.f, 46},
			{"Bip01 L Toe0", 0.f, 47},	   {"Bip01 R Thigh", 0.f, 1},	  {"Bip01 R Calf", 0.f, 49},	 {"Bip01 R Foot", 0.f, 50},
			{"Bip01 R Toe0", 0.f, 51},
		};
		return bones;
	}

	static bool equalsIgnoreCase(const std::string &a, const std::string &b)
	{
		return a.size() == b.size()
			   && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(x) == std::tolower(y); });
	}

	static int findDefaultBone(const std::string &name)
	{
		const auto &bones = defaultArmature();
		for (size_t i = 0; i < bones.size(); i++)
		{
			if (equalsIgnoreCase(name, bones[i].name))
			{
				return int(i);
			}
		}
		return -1;
	}

	void applyDefaultJointInformation(SkeletalModel &model)
	{
		model.torsoParent = -1;

		for (size_t i = 0; i < model.joints.size(); i++)
		{
			int bone = findDefaultBone(model.joints[i].name);
			if (bone < 0)
			{
				continue;
			}

			model.joints[i].torsoWeight = defaultArmature()[bone].torsoWeight;
			if (bone == DEFAULT_TORSO_PARENT)
			{
				model.torsoParent = int(i);
			}
		}

		if (model.torsoParent < 0)
		{
			auto root = std::find_if(model.joints.begin(), model.joints.end(), [](const Joint &joint) { return joint.parentId < 0; });
			if (root != model.joints.end())
			{
				model.torsoParent = int(root - model.joints.begin());
			}
		}
	}

	void markTagBones(SkeletalModel &model)
	{
		for (size_t i = 0; i < model.joints.size(); i++)
		{
			const int index = int(i);
			bool hasChildren
				= std::any_of(model.joints.begin(), model.joints.end(), [&](const Joint &joint) { return joint.parentId == index; })
				  || std::any_of(model.tags.begin(), model.tags.end(), [&](const BoneTag &tag) { return tag.parentBone == index; });

			std::string prefix = model.joints[i].name.substr(0, 4);
			if (!hasChildren && equalsIgnoreCase(prefix, "tag_"))
			{
				model.joints[i].asTag = true;
			}
		}
	}

	std::vector<std::string> checkArmature(const SkeletalModel &model)
	{
		std::vector<std::string> issues;
		const auto &bones = defaultArmature();

		std::vector<std::string> roots;
		size_t boneCount = 0;
		for (const auto &joint : model.joints)
		{
			if (joint.asTag)
			{
				continue;
			}

			boneCount++;
			if (joint.parentId < 0)
			{
				roots.push_back(joint.name);
			}
		}

		if (roots.size() != 1)
		{
			std::string names;
			for (const auto &root : roots)
			{
				names += (names.empty() ? "" : ", ") + root;
			}
			issues.push_back("The armature has " + std::to_string(roots.size()) + " root bones, the game expects 1"
							 + (names.empty() ? "" : ": " + names));
		}

		if (boneCount != bones.size())
		{
			issues.push_back("The armature has " + std::to_string(boneCount) + " bones, the default has " + std::to_string(bones.size()));
		}

		for (const auto &bone : bones)
		{
			auto joint = std::find_if(model.joints.begin(), model.joints.end(),
									  [&](const Joint &j) { return !j.asTag && equalsIgnoreCase(j.name, bone.name); });

			if (joint == model.joints.end())
			{
				issues.push_back(std::string("Bone not found: ") + bone.name);
				continue;
			}

			std::string parent = joint->parentId >= 0 ? model.joints[joint->parentId].name : "";
			std::string defaultParent = bone.parentId >= 0 ? bones[bone.parentId].name : "";
			if (!equalsIgnoreCase(parent, defaultParent))
			{
				issues.push_back(std::string("Bone ") + bone.name + " has the parent '" + parent + "', the default is '" + defaultParent
								 + "'");
			}
		}

		return issues;
	}

	int reorganizeArmature(SkeletalModel &model)
	{
		std::vector<int> order;
		std::vector<bool> used(model.joints.size(), false);

		for (const auto &bone : defaultArmature())
		{
			for (size_t i = 0; i < model.joints.size(); i++)
			{
				if (!used[i] && !model.joints[i].asTag && equalsIgnoreCase(model.joints[i].name, bone.name))
				{
					order.push_back(int(i));
					used[i] = true;
					break;
				}
			}
		}

		for (size_t i = 0; i < model.joints.size(); i++)
		{
			if (!used[i])
			{
				order.push_back(int(i));
			}
		}

		int moved = 0;
		for (size_t i = 0; i < order.size(); i++)
		{
			moved += order[i] != int(i) ? 1 : 0;
		}

		skeletal::reorderJoints(model, order);
		return moved;
	}
}
