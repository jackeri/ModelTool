#include "SkeletalModel.h"
#include "renderer.h"

namespace mt::model::Skeletal {

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

	void SkeletalModel::renderModel(Ref<mt::Renderer> renderer)
	{
		auto &buffer = renderer->getBuffer();

		bool drawBindPose = false;

		if (!this->hasFrames() || this->currentFrame == FRAME_BIND_POSE)
		{
			drawBindPose = true;

			if (!this->hasBindPose)
			{
				// FIXME: output an error or a log message? We have nothing to draw..
				return;
			}
		}

		// This isn't particularly fast, but it doesn't really matter in this case
		for (auto &mesh : this->meshes)
		{
			buffer.clear();
			buffer.color = {1, 1, 1, 1};

			for (auto &vert : mesh.vertices)
			{
				if (drawBindPose)
				{
					buffer.addXyz(vert.pos);
					buffer.addNormal(vert.bindNormal);
				}
				else
				{
					glm::fvec3 pos(0), normal(0);
					for (int i = 0; i < vert.weightCount; i++)
					{
						const Weight &weight = mesh.weights[vert.startWeight + i];
						const Point &point = joints[weight.jointId].frames[currentFrame];

						glm::vec3 rotatedPos = point.rotation * weight.pos;
						pos += (point.location + rotatedPos) * weight.bias;

						rotatedPos = point.rotation * vert.normal;
						normal += rotatedPos * weight.bias;
					}
					buffer.addXyz(pos);
					buffer.addNormal(normal);
				}
			}

			for (auto &tri : mesh.tris)
			{
				buffer.addIndex(tri);
			}

			renderer->renderBuffer();
		}

		for (auto &hitbox : hitboxes)
		{
			glm::mat4x4 matrix{0};

			if (hitbox.parentJoint >= 0)
			{
				const Point& joint = (drawBindPose ? joints[hitbox.parentJoint] : joints[hitbox.parentJoint].frames[currentFrame]);
				glm::vec3 rotPoint = joint.location + (joint.rotation * hitbox.location);
				matrix = glm::translate(rotPoint) * glm::toMat4(joint.rotation * hitbox.rotation);
			}
			else
			{
				matrix = glm::translate(hitbox.location) * glm::toMat4(hitbox.rotation);
			}

			setupBounds(renderer, hitbox);

			// Push the hitbox matrix to the renderer
			renderer->push(matrix);
			renderer->renderBuffer();
			renderer->pop();
		}
	}
}
