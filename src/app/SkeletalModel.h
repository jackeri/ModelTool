#pragma once

#include "mt.h"
#include "Model.h"

namespace mt::model::Skeletal {

	struct Joint : public Point {
		std::string name{};
		int parentId{};
		int flags{};
		int startIndex{};
		float torsoWeight{}; // FIXME: compute value
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
			if (!VectorEqual(m_Pos, rhs.m_Pos))
			{
				return false;
			}

			if (!Vector2Equal(m_Tex0, rhs.m_Tex0))
			{
				return false;
			}

			if (/*m_StartWeight != rhs.m_StartWeight ||*/ m_WeightCount != rhs.m_WeightCount)
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
		int m_JointID;
		float m_Bias;
		vec3_t m_Pos;

		bool operator==(const Weight &rhs) const
		{
			return (m_JointID == rhs.m_JointID && m_Bias == rhs.m_Bias && VectorEqual(m_Pos, rhs.m_Pos));
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
		std::vector<Vertex> vertices;
		std::vector<glm::ivec3> tris;
		std::vector<Weight> weights;

		int minLod = 0;

		Material *material = nullPtr;

		std::vector<int> collapseMap; // Every mesh has a map

		Mesh()
		{
		}

		~Mesh()
		{
			emptyVector(m_Verts);
			emptyVector(m_Tris);
			emptyVector(m_Weights);
		}
	};

	class SkeletalModel {
	public:

	};

}