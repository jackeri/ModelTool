#pragma once

#include "library.h"
#include "model.h"
#include "hitbox.h"

namespace mt::model::skeletal {

	/*! Skeletons joint */
	struct Joint : public Point {
		std::string name{}; ///< name of the bone
		int parentId{};		///< parent bone offset

		bool asTag{};			   ///< can this bone be used as a tag (MDS/MDX BONEFLAG_TAG)
		float torsoWeight{};	   ///< how much the torso rotation (around the model's torso parent) affects this bone
		float parentDist{};		   ///< distance to the parent bone (MDS/MDX)
		std::vector<Point> frames; ///< The frame's pre calculated joints

		// FIXME: remove these, these are really only MD5 related
		int flags{}; ///< bone flags
		int startIndex{};

		Joint() : Point() {}

		~Joint()
		{
			emptyVector(frames);
		}

		/**
		 * Return the joint information for the frame
		 * @param offset frame offset
		 * @return joint data for the frame
		 */
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
		{
			return frame(i);
		}
	};

	/*! Skeletal models vertex which holds weight information */
	struct SkeletalVertex : public Vertex {
		int startWeight{};		///< Weight offset
		int weightCount{};		///< weight count
		glm::vec3 bindNormal{}; ///< bind pose normal for this vertex

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

	/*! Tag that is bound to a bone, location and rotation are relative to the parent bone */
	struct BoneTag : public Point {
		std::string name{};	 ///< name of the tag
		int parentBone = -1; ///< Parent bone offset
		float torsoWeight{}; ///< torso rotation weight (MDS tags)
	};

	/*! Bone weight */
	struct Weight {
		int jointId{};	 ///< bone offset
		float bias{};	 ///< bias value (how much does it affect the output)
		glm::vec3 pos{}; ///< relative position

		bool operator==(const Weight &rhs) const
		{
			return (jointId == rhs.jointId && bias == rhs.bias && pos == rhs.pos);
		}

		bool operator!=(const Weight &rhs) const
		{
			return !(*this == rhs);
		}
	};

	/*! Skeletal models mesh */
	struct Mesh {
		std::string name{};						///< name of the mesh
		std::string materialName{};				///< name of the material for the mesh
		std::vector<SkeletalVertex> vertices{}; ///< This vertex list stores the vertices in the bind pose.
		std::vector<glm::ivec3> tris{};			///< mesh triangles
		std::vector<Weight> weights{};			///< mesh weights
		int minLod = 0;							///< lodding level of the mesh
		reshandle material{};					///< material instance used in this mesh

		// FIXME: this will be used by the ET models, implement later
		std::vector<int> collapseMap; ///< Every skeletal mesh has a collapse map that is used for dynamic lodding (level of detail)

		Mesh() = default;

		~Mesh()
		{
			emptyVector(vertices);
			emptyVector(tris);
			emptyVector(weights);
		}
	};

	/*! Model with a skeleton managing the vertices positions */
	struct SkeletalModel : public Model {
		std::vector<Joint> joints{};	///< model bones
		std::vector<Mesh> meshes{};		///< model meshes
		std::vector<BoneTag> tags{};	///< model tags, which can be bound to other tags or bones
		std::vector<Bounds> bounds{};	///< models bounds
		std::vector<Hitbox> hitboxes{}; ///< models hitboxes which can be bound to tags or bones
		bool hasBindPose = false;		///< defines if the loaded model has a bind pose setup
		int torsoParent = -1;			///< bone which is the parent of the torso (MDS/MDX), -1 if not set
		float lodScale = 1.0f;			///< level of detail scale (MDS/MDM)
		float lodBias = 0.0f;			///< level of detail bias (MDS/MDM)

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

		bool hasTags() override;

		// TODO: needed when doing bone scaling in the future
		int findParentIndex(int boneIndex);
	};
}
