#include "posed_model.h"
#include "skeletal_model.h"
#include "morph_model.h"

namespace mt::model {
	using skeletal::Joint;
	using skeletal::SkeletalModel;

	namespace {
		Point blendPoints(const Point &a, const Point &b, float t)
		{
			if (t <= 0.0f)
			{
				return a;
			}
			return {glm::mix(a.location, b.location, t), glm::normalize(glm::slerp(a.rotation, b.rotation, t))};
		}

		glm::vec3 normalized(const glm::vec3 &vec)
		{
			float length = glm::length(vec);
			return length > 0.0f ? vec / length : vec;
		}

		Point combine(const Point &parent, const Point &child)
		{
			return {parent.location + parent.rotation * child.location, glm::normalize(parent.rotation * child.rotation)};
		}

		int clampFrame(int frame, int numFrames)
		{
			return numFrames > 0 ? std::clamp(frame, 0, numFrames - 1) : 0;
		}

		std::vector<glm::ivec3> meshTriangles(const std::vector<glm::ivec3> &tris, const std::vector<int> &collapseMap, int numVertices,
											  int minLod, const PoseOptions &options)
		{
			if (!options.dynamicLod || collapseMap.empty())
			{
				return tris;
			}
			return lodTriangles(tris, collapseMap, lodVertexCount(numVertices, minLod, options.lodRatio));
		}

		PosedModel poseSkeletal(const SkeletalModel &model, const PoseOptions &options)
		{
			PosedModel out;

			const int numFrames = model.joints.empty() ? 0 : int(model.joints[0].frames.size());
			const bool bind = options.bindPose || numFrames == 0;
			if (bind && !model.hasBindPose)
			{
				return out;
			}

			const int frame = clampFrame(options.frame, numFrames);
			const int next = clampFrame(options.nextFrame, numFrames);

			std::vector<Point> pose;
			pose.reserve(model.joints.size());
			for (const auto &joint : model.joints)
			{
				if (bind || joint.frames.empty())
				{
					pose.push_back(joint);
				}
				else
				{
					pose.push_back(blendPoints(joint.frames[frame], joint.frames[next], options.blend));
				}
			}

			for (const auto &mesh : model.meshes)
			{
				PosedMesh posed;
				posed.name = mesh.name;
				posed.material = mesh.materialName;

				for (const auto &vert : mesh.vertices)
				{
					posed.texCoords.push_back(vert.tex);

					if (bind || vert.weightCount <= 0)
					{
						posed.positions.push_back(vert.pos);
						posed.normals.push_back(vert.bindNormal);
						continue;
					}

					glm::vec3 pos{0}, normal{0};
					for (int i = 0; i < vert.weightCount; i++)
					{
						const auto &weight = mesh.weights.at(size_t(vert.startWeight + i));
						const Point &joint = pose.at(size_t(weight.jointId));
						pos += (joint.location + joint.rotation * weight.pos) * weight.bias;
						normal += (joint.rotation * vert.normal) * weight.bias;
					}
					posed.positions.push_back(pos);
					posed.normals.push_back(normalized(normal));
				}

				posed.tris = meshTriangles(mesh.tris, mesh.collapseMap, int(mesh.vertices.size()), mesh.minLod, options);
				out.meshes.push_back(std::move(posed));
			}

			for (size_t i = 0; i < model.joints.size(); i++)
			{
				const Joint &joint = model.joints[i];
				if (joint.asTag)
				{
					Point point = pose[i];
					if (model.flipBoneTags)
					{
						point.rotation = glm::normalize(point.rotation * BONE_TAG_FLIP);
					}
					out.tags.push_back({joint.name, point});
					continue;
				}

				out.joints.push_back(pose[i].location);
				if (joint.parentId >= 0 && !model.joints[joint.parentId].asTag)
				{
					out.bones.emplace_back(pose[i].location, pose[joint.parentId].location);
				}
			}

			for (const auto &tag : model.tags)
			{
				if (tag.parentBone < 0 || tag.parentBone >= int(pose.size()))
				{
					continue;
				}
				out.tags.push_back({tag.name, combine(pose[tag.parentBone], tag)});
			}

			for (const auto &hitbox : model.hitboxes)
			{
				Point point = hitbox;
				if (hitbox.parentJoint >= 0 && hitbox.parentJoint < int(pose.size()))
				{
					point = combine(pose[hitbox.parentJoint], hitbox);
				}
				out.hitboxes.push_back({point, hitbox});
			}

			if (!bind && frame < int(model.bounds.size()))
			{
				out.hasBounds = true;
				out.bounds = model.bounds[frame];
			}

			return out;
		}

		PosedModel poseMorph(const morph::MorphModel &model, const PoseOptions &options)
		{
			PosedModel out;
			const int numFrames = int(model.frames.size());
			if (numFrames == 0)
			{
				return out;
			}

			const int frame = clampFrame(options.frame, numFrames);
			const int next = clampFrame(options.nextFrame, numFrames);
			const float t = options.blend;

			const int level = std::clamp(options.lodLevel, 0, model.lodCount() - 1);
			const auto &meshes = level == 0 ? model.meshes : model.lods[level - 1];

			for (const auto &mesh : meshes)
			{
				PosedMesh posed;
				posed.name = mesh.name;
				posed.material = mesh.materialName;
				posed.texCoords = mesh.texCoords;

				const auto &a = mesh.frames.at(size_t(frame));
				const auto &b = mesh.frames.at(size_t(next));
				for (size_t v = 0; v < a.size(); v++)
				{
					if (t <= 0.0f)
					{
						posed.positions.push_back(a[v].pos);
						posed.normals.push_back(a[v].normal);
					}
					else
					{
						posed.positions.push_back(glm::mix(a[v].pos, b[v].pos, t));
						posed.normals.push_back(normalized(glm::mix(a[v].normal, b[v].normal, t)));
					}
				}

				posed.tris = meshTriangles(mesh.tris, mesh.collapseMap, int(mesh.numVertices()), mesh.minLod, options);
				out.meshes.push_back(std::move(posed));
			}

			for (const auto &tag : model.tags)
			{
				out.tags.push_back({tag.name, blendPoints(tag.frames.at(size_t(frame)), tag.frames.at(size_t(next)), t)});
			}

			out.hasBounds = true;
			out.bounds = model.frames[frame].bounds;
			return out;
		}
	}

	int lodVertexCount(int numVertices, int minLod, float ratio)
	{
		minLod = std::clamp(minLod, 0, numVertices);
		ratio = std::clamp(ratio, 0.0f, 1.0f);
		return minLod + int(std::lround(float(numVertices - minLod) * ratio));
	}

	std::vector<glm::ivec3> lodTriangles(const std::vector<glm::ivec3> &tris, const std::vector<int> &collapseMap, int vertexCount)
	{
		const int numVertices = int(collapseMap.size());
		if (collapseMap.empty() || vertexCount >= numVertices)
		{
			return tris;
		}

		auto map = [&](int index) {
			// Every collapse step goes to a lower vertex, the loop guard protects against broken maps
			for (int steps = 0; index >= vertexCount && steps < numVertices; steps++)
			{
				if (index < 0 || index >= numVertices)
				{
					return -1;
				}
				index = collapseMap[index];
			}
			return index >= 0 && index < vertexCount ? index : -1;
		};

		std::vector<glm::ivec3> out;
		for (const auto &tri : tris)
		{
			glm::ivec3 mapped{map(tri.x), map(tri.y), map(tri.z)};
			if (mapped.x < 0 || mapped.y < 0 || mapped.z < 0 || mapped.x == mapped.y || mapped.y == mapped.z || mapped.z == mapped.x)
			{
				continue;
			}
			out.push_back(mapped);
		}
		return out;
	}

	PosedModel poseModel(const Model &model, const PoseOptions &options)
	{
		if (const auto *skeletal = dynamic_cast<const SkeletalModel *>(&model))
		{
			return poseSkeletal(*skeletal, options);
		}

		if (const auto *morph = dynamic_cast<const morph::MorphModel *>(&model))
		{
			return poseMorph(*morph, options);
		}

		return {};
	}
}
