#include "renderer.h"
#include "skeletal_model.h"
#include "model.h"

using namespace mt;
using namespace mt::model;

static void renderSkeletalModel(const Ref<mt::Renderer> &renderer, const Ref<mt::model::skeletal::SkeletalModel> &model)
{
	auto &buffer = renderer->getBuffer();

	bool drawBindPose = false;

	if (!model->hasFrames() || model->currentFrame == FRAME_BIND_POSE)
	{
		drawBindPose = true;

		if (!model->hasBindPose)
		{
			// FIXME: output an error or a log message? We have nothing to draw..
			return;
		}
	}

	// model isn't particularly fast, but it doesn't really matter in model case
	for (auto &mesh : model->meshes)
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
					const skeletal::Weight &weight = mesh.weights[vert.startWeight + i];
					const Point &point = model->joints[weight.jointId].frames[model->currentFrame];

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

	for (auto &hitbox : model->hitboxes)
	{
		glm::mat4x4 matrix{0};

		if (hitbox.parentJoint >= 0)
		{
			const Point &joint
				= (drawBindPose ? model->joints[hitbox.parentJoint] : model->joints[hitbox.parentJoint].frames[model->currentFrame]);
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

void mt::model::renderModel(const Ref<mt::Renderer> &renderer, const Ref<mt::Model> &model)
{
	if (model->skeletalModel())
	{
		auto skeletalModel = std::dynamic_pointer_cast<mt::model::skeletal::SkeletalModel>(model);
		renderSkeletalModel(renderer, skeletalModel);
	}
}

void mt::model::setupBounds(const Ref<mt::Renderer> &renderer, const mt::Bounds &bounds, bool drawPoints)
{
	auto &buffer = renderer->getBuffer();
	buffer.clear(false);

	// Green boxes
	buffer.color = {0, 1, 0, 1};
	buffer.lineWidth = 2;

	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2
	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3

	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3
	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0

	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.max.z}); // 3
	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4

	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4
	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7

	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.max.z}); // 4
	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5

	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5
	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2

	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.max.z}); // 5
	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6

	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6
	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1

	buffer.addXyz({bounds.max.x, bounds.max.y, bounds.min.z}); // 6
	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7

	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0
	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1

	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.min.z}); // 1
	buffer.addXyz({bounds.max.x, bounds.min.y, bounds.max.z}); // 2

	buffer.addXyz({bounds.min.x, bounds.max.y, bounds.min.z}); // 7
	buffer.addXyz({bounds.min.x, bounds.min.y, bounds.min.z}); // 0

	// FIXME: implement point drawing
}
