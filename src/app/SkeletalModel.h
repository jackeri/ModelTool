#pragma once

#include "mt.h"
#include "Model.h"
#include "Material.h"
#include "Hitbox.h"

namespace mt::model::Skeletal {

	struct Joint : public Point {
		std::string name{};
		int parentId{};
		int flags{};
		int startIndex{};
		float torsoWeight{};
		bool asTag{};
		bool possibleTag{};

		// The frame's pre calculated joints
		std::vector<Point> frames;

		Joint() : Point()
		{

		}

		~Joint()
		{
			emptyVector(frames);
		}

		Point &frame(int offset = -1)
		{
			if (offset == -1)
			{
				return *this;
			}
			else
			{
				return frames[offset];
			}
		}

		Point &operator[](int i)
		{ return frame(i); }
	};

	struct SkeletalVertex : public Vertex {
		int startWeight{};
		int weightCount{};
		glm::vec3 bindNormal{};

		SkeletalVertex() : Vertex()
		{
		}

		SkeletalVertex(const SkeletalVertex &other) : Vertex(other)
		{
			startWeight = other.startWeight;
			weightCount = other.weightCount;
			bindNormal = other.bindNormal;
			*this = other;
		}

		SkeletalVertex &operator=(const SkeletalVertex &other)
		{

			return *this;
		}

		bool operator==(const SkeletalVertex &rhs) const
		{
			if (pos != rhs.pos)
			{
				return false;
			}

			if (tex != rhs.tex)
			{
				return false;
			}

			if (weightCount != rhs.weightCount)
			{
				return false;
			}

			return true;
		}
	};

	struct BoneTag : public Point {
		int parentBone;

		BoneTag() : Point()
		{
			this->parentBone = -1;
		}
	};

	struct Weight {
		int jointId{};
		float bias{};
		glm::vec3 pos{};

		bool operator==(const Weight &rhs) const
		{
			return (jointId == rhs.jointId && bias == rhs.bias && pos == rhs.pos);
		}

		bool operator!=(const Weight &rhs) const
		{
			return !(*this == rhs);
		}
	};

	struct Mesh {
		std::string name{};
		std::string materialName{};
		// This vertex list stores the vertices in the bind pose.
		std::vector<SkeletalVertex> vertices{};
		std::vector<glm::ivec3> tris{};
		std::vector<Weight> weights{};

		int minLod = 0;

		Material material{};

		// FIXME: this will be used by the ET models, implement later
		std::vector<int> collapseMap; // Every mesh has a map

		Mesh() = default;

		~Mesh()
		{
			emptyVector(vertices);
			emptyVector(tris);
			emptyVector(weights);
		}
	};

	class SkeletalModel : public Model {
	public:

		std::vector<Joint> joints{};
		std::vector<Mesh> meshes{};
		std::vector<BoneTag> tags{};

		std::vector<Bounds> bounds{};

		std::vector<Hitbox> hitboxes{};

		bool hasBindPose = false;

		~SkeletalModel() override;

		int numFrames() override;

		bool skeletalModel() override
		{
			return true;
		}

		std::vector<mt::model::Hitbox> *getHitboxes() override
		{
			return &hitboxes;
		}

		bool hasTags();

		int findParentIndex(int boneIndex);

		void renderModel(Ref<mt::Renderer>) override;

	};

}