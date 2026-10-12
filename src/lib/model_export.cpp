#include "model_export.h"
#include "skeletal_tools.h"
#include "posed_model.h"

namespace mt::model {
	using namespace mt::model::skeletal;

	std::unique_ptr<Model> cloneModel(const Model &model)
	{
		if (const auto *skeletal = dynamic_cast<const SkeletalModel *>(&model))
		{
			return std::make_unique<SkeletalModel>(*skeletal);
		}
		if (const auto *morph = dynamic_cast<const morph::MorphModel *>(&model))
		{
			return std::make_unique<morph::MorphModel>(*morph);
		}
		throw mt_ex("Unsupported model type");
	}

	namespace {
		struct Transform {
			glm::vec3 location;
			glm::quat rotation;
			float scale;

			[[nodiscard]] glm::vec3 point(const glm::vec3 &pos) const
			{
				return location + rotation * (pos * scale);
			}

			[[nodiscard]] Point pose(const Point &value) const
			{
				return {point(value.location), glm::normalize(rotation * value.rotation)};
			}

			[[nodiscard]] Bounds bounds(const Bounds &box) const
			{
				Bounds out{};
				for (int corner = 0; corner < 8; corner++)
				{
					glm::vec3 pos{corner & 1 ? box.max.x : box.min.x, corner & 2 ? box.max.y : box.min.y,
								  corner & 4 ? box.max.z : box.min.z};
					pos = point(pos);
					out.min = corner ? glm::min(out.min, pos) : pos;
					out.max = corner ? glm::max(out.max, pos) : pos;
				}
				return out;
			}
		};

		void transformMorphMeshes(std::vector<morph::Mesh> &meshes, const Transform &transform)
		{
			for (auto &mesh : meshes)
			{
				for (auto &frame : mesh.frames)
				{
					for (auto &vert : frame)
					{
						vert.pos = transform.point(vert.pos);
						vert.normal = transform.rotation * vert.normal;
					}
				}
			}
		}

		template<typename T>
		void cropVector(std::vector<T> &values, int first, int last)
		{
			if (values.empty())
			{
				return;
			}
			first = std::clamp(first, 0, int(values.size()) - 1);
			last = std::clamp(last, first, int(values.size()) - 1);
			values = std::vector<T>(values.begin() + first, values.begin() + last + 1);
		}
	}

	void bakeTransform(Model &model)
	{
		const Transform transform{model.transform.location, glm::normalize(model.transform.rotation), model.scale};

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			for (auto &joint : skeletal->joints)
			{
				static_cast<Point &>(joint) = transform.pose(joint);
				for (auto &frame : joint.frames)
				{
					frame = transform.pose(frame);
				}
				joint.parentDist *= transform.scale;
			}

			for (auto &mesh : skeletal->meshes)
			{
				for (auto &vert : mesh.vertices)
				{
					vert.pos = transform.point(vert.pos);
					vert.bindNormal = transform.rotation * vert.bindNormal;
				}
				for (auto &weight : mesh.weights)
				{
					weight.pos *= transform.scale;
				}
			}

			for (auto &tag : skeletal->tags)
			{
				tag.location *= transform.scale;
			}

			for (auto &hitbox : skeletal->hitboxes)
			{
				hitbox.min *= transform.scale;
				hitbox.max *= transform.scale;
				if (hitbox.parentJoint >= 0)
				{
					hitbox.location *= transform.scale;
				}
				else
				{
					static_cast<Point &>(hitbox) = transform.pose(hitbox);
				}
			}

			for (auto &bounds : skeletal->bounds)
			{
				bounds = transform.bounds(bounds);
			}
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			transformMorphMeshes(morph->meshes, transform);
			for (auto &level : morph->lods)
			{
				transformMorphMeshes(level, transform);
			}

			for (auto &tag : morph->tags)
			{
				for (auto &frame : tag.frames)
				{
					frame = transform.pose(frame);
				}
			}

			for (auto &frame : morph->frames)
			{
				frame.bounds = transform.bounds(frame.bounds);
				frame.localOrigin = (frame.bounds.min + frame.bounds.max) * 0.5f;
				frame.radius = glm::distance(frame.localOrigin, frame.bounds.max);
			}
		}

		model.transform = Point{};
		model.scale = 1.0f;
	}

	void cropFrames(Model &model, int first, int last)
	{
		const int numFrames = model.numFrames();
		if (numFrames == 0)
		{
			return;
		}

		first = std::clamp(first, 0, numFrames - 1);
		last = std::clamp(last < 0 ? numFrames - 1 : last, first, numFrames - 1);
		if (first == 0 && last == numFrames - 1)
		{
			return;
		}

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			for (auto &joint : skeletal->joints)
			{
				cropVector(joint.frames, first, last);
			}
			cropVector(skeletal->bounds, first, last);
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			cropVector(morph->frames, first, last);
			for (auto &mesh : morph->meshes)
			{
				cropVector(mesh.frames, first, last);
			}
			for (auto &level : morph->lods)
			{
				for (auto &mesh : level)
				{
					cropVector(mesh.frames, first, last);
				}
			}
			for (auto &tag : morph->tags)
			{
				cropVector(tag.frames, first, last);
			}
		}

		std::vector<Animation> animations;
		for (auto animation : model.animations)
		{
			int low = std::min(animation.startFrame, animation.lastFrame());
			int high = std::max(animation.startFrame, animation.lastFrame());
			low = std::max(low, first);
			high = std::min(high, last);
			if (low > high)
			{
				continue;
			}

			animation.numFrames = high - low + 1;
			animation.startFrame = (animation.reversed ? high : low) - first;
			animations.push_back(animation);
		}
		model.animations = std::move(animations);

		if (model.currentFrame >= 0)
		{
			model.currentFrame = std::clamp(model.currentFrame - first, 0, last - first);
		}
	}

	std::unique_ptr<Model> prepareForExport(const Model &model)
	{
		auto out = cloneModel(model);
		bakeTransform(*out);
		cropFrames(*out, out->firstExportFrame, out->lastExportFrame);
		out->firstExportFrame = 0;
		out->lastExportFrame = -1;
		return out;
	}

	std::vector<bool> tagBones(const SkeletalModel &model)
	{
		std::vector<bool> out(model.joints.size(), false);
		for (size_t i = 0; i < model.joints.size(); i++)
		{
			out[i] = model.joints[i].asTag && model.joints[i].parentId >= 0 && canBeTag(model, int(i)) && model.torsoParent != int(i);
		}
		return out;
	}

	BoneTag boneTagFromJoint(const SkeletalModel &model, int index)
	{
		const Joint &joint = model.joints.at(size_t(index));
		const Joint &parentJoint = model.joints.at(size_t(joint.parentId));

		const bool useBind = model.hasBindPose || joint.frames.empty() || parentJoint.frames.empty();
		const Point &pose = useBind ? static_cast<const Point &>(joint) : joint.frames.front();
		const Point &parent = useBind ? static_cast<const Point &>(parentJoint) : parentJoint.frames.front();

		BoneTag tag;
		tag.name = joint.name;
		tag.parentBone = joint.parentId;
		tag.torsoWeight = joint.torsoWeight;
		tag.location = glm::inverse(parent.rotation) * (pose.location - parent.location);
		tag.rotation = glm::normalize(glm::inverse(parent.rotation) * pose.rotation);
		if (model.flipBoneTags)
		{
			tag.rotation = glm::normalize(tag.rotation * BONE_TAG_FLIP);
		}
		return tag;
	}

	std::unique_ptr<morph::MorphModel> toMorphModel(const SkeletalModel &model)
	{
		const int numFrames = model.joints.empty() ? 0 : int(model.joints[0].frames.size());
		if (numFrames == 0 && !model.hasBindPose)
		{
			throw mt_ex("The model has no frames and no bind pose");
		}

		auto out = std::make_unique<morph::MorphModel>();
		out->name = model.name;
		out->animations = model.animations;
		const int frames = std::max(numFrames, 1);
		out->frames.resize(size_t(frames));

		for (int frame = 0; frame < frames; frame++)
		{
			PoseOptions options;
			options.bindPose = numFrames == 0;
			options.frame = options.nextFrame = frame;
			PosedModel posed = poseModel(model, options);

			for (size_t m = 0; m < posed.meshes.size(); m++)
			{
				const auto &source = posed.meshes[m];
				if (frame == 0)
				{
					morph::Mesh mesh;
					mesh.name = source.name;
					mesh.materialName = source.material;
					mesh.texCoords = source.texCoords;
					mesh.tris = model.meshes[m].tris;
					out->meshes.push_back(std::move(mesh));
				}

				std::vector<morph::FrameVertex> vertices(source.positions.size());
				for (size_t v = 0; v < vertices.size(); v++)
				{
					vertices[v] = {source.positions[v], source.normals[v]};
				}
				out->meshes[m].frames.push_back(std::move(vertices));
			}

			for (size_t t = 0; t < posed.tags.size(); t++)
			{
				if (frame == 0)
				{
					out->tags.push_back({posed.tags[t].name, {}});
				}
				out->tags[t].frames.push_back(posed.tags[t].point);
			}

			Bounds bounds{};
			bool any = false;
			for (const auto &mesh : posed.meshes)
			{
				for (const auto &pos : mesh.positions)
				{
					bounds.min = any ? glm::min(bounds.min, pos) : pos;
					bounds.max = any ? glm::max(bounds.max, pos) : pos;
					any = true;
				}
			}
			auto &info = out->frames[frame];
			info.bounds = bounds;
			info.localOrigin = (bounds.min + bounds.max) * 0.5f;
			info.radius = glm::distance(info.localOrigin, bounds.max);
		}

		out->validate();
		return out;
	}
}
