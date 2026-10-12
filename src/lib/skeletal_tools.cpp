#include "skeletal_tools.h"
#include "morph_model.h"
#include "normals.h"

#include <map>
#include <tuple>

namespace mt::model::skeletal {

	glm::vec3 posedVertex(const SkeletalModel &model, const Mesh &mesh, const SkeletalVertex &vert, int frame)
	{
		if (vert.weightCount <= 0)
		{
			return vert.pos;
		}

		glm::vec3 pos{0, 0, 0};
		for (int i = 0; i < vert.weightCount; i++)
		{
			const Weight &weight = mesh.weights.at(size_t(vert.startWeight + i));
			const Joint &joint = model.joints.at(size_t(weight.jointId));
			const Point &pose = frame == FRAME_BIND_POSE ? static_cast<const Point &>(joint) : joint.frames.at(size_t(frame));
			pos += (pose.location + pose.rotation * weight.pos) * weight.bias;
		}
		return pos;
	}

	void calculateBounds(SkeletalModel &model, float scale, int startFrame, int endFrame)
	{
		const int numFrames = model.numFrames();
		if (endFrame < 0 || endFrame > numFrames)
		{
			endFrame = numFrames;
		}

		model.bounds.resize(size_t(numFrames));

		for (int frame = std::max(startFrame, 0); frame < endFrame; frame++)
		{
			glm::vec3 min{std::numeric_limits<float>::max()};
			glm::vec3 max{std::numeric_limits<float>::lowest()};
			bool any = false;

			for (const auto &mesh : model.meshes)
			{
				for (const auto &vert : mesh.vertices)
				{
					glm::vec3 pos = posedVertex(model, mesh, vert, frame);
					min = glm::min(min, pos);
					max = glm::max(max, pos);
					any = true;
				}
			}

			model.bounds[frame] = any ? Bounds{min * scale, max * scale} : Bounds{};
		}
	}

	using VertexKey = std::tuple<std::array<float, 3>, std::array<float, 2>, std::array<float, 3>,
								 std::vector<std::tuple<int, float, std::array<float, 3>>>>;

	static std::array<float, 3> toArray(const glm::vec3 &vec)
	{
		return {vec.x, vec.y, vec.z};
	}

	size_t optimizeVertices(SkeletalModel &model)
	{
		size_t removed = 0;

		for (auto &mesh : model.meshes)
		{
			std::map<VertexKey, int> unique;
			std::vector<int> remap(mesh.vertices.size());
			std::vector<SkeletalVertex> vertices;
			std::vector<Weight> weights;

			for (size_t v = 0; v < mesh.vertices.size(); v++)
			{
				const SkeletalVertex &vert = mesh.vertices[v];

				std::vector<std::tuple<int, float, std::array<float, 3>>> vertexWeights;
				for (int i = 0; i < vert.weightCount; i++)
				{
					const Weight &weight = mesh.weights.at(size_t(vert.startWeight + i));
					vertexWeights.emplace_back(weight.jointId, weight.bias, toArray(weight.pos));
				}

				VertexKey key{toArray(vert.pos), {vert.tex.x, vert.tex.y}, toArray(vert.bindNormal), vertexWeights};
				auto found = unique.find(key);
				if (found != unique.end())
				{
					remap[v] = found->second;
					continue;
				}

				SkeletalVertex copy = vert;
				copy.startWeight = int(weights.size());
				for (int i = 0; i < vert.weightCount; i++)
				{
					weights.push_back(mesh.weights[size_t(vert.startWeight + i)]);
				}

				remap[v] = int(vertices.size());
				unique.emplace(std::move(key), remap[v]);
				vertices.push_back(copy);
			}

			if (vertices.size() == mesh.vertices.size())
			{
				continue;
			}

			removed += mesh.vertices.size() - vertices.size();

			for (auto &tri : mesh.tris)
			{
				for (int i = 0; i < 3; i++)
				{
					tri[i] = remap.at(size_t(tri[i]));
				}
			}

			mesh.vertices = std::move(vertices);
			mesh.weights = std::move(weights);
			mesh.collapseMap.clear();
			mesh.minLod = std::min(mesh.minLod, int(mesh.vertices.size()));
		}

		return removed;
	}

	void bakeBindPose(SkeletalModel &model, int frame, bool armature)
	{
		if (armature && (frame < 0 || frame >= model.numFrames()))
		{
			throw mt_ex("Invalid frame to bake: " + std::to_string(frame));
		}

		// Positions in the new bind pose, calculated before the joints change
		const int pose = armature ? frame : FRAME_BIND_POSE;
		std::vector<std::vector<glm::vec3>> positions;
		for (const auto &mesh : model.meshes)
		{
			std::vector<glm::vec3> meshPositions;
			for (const auto &vert : mesh.vertices)
			{
				meshPositions.push_back(posedVertex(model, mesh, vert, pose));
			}
			positions.push_back(std::move(meshPositions));
		}

		if (armature)
		{
			for (auto &joint : model.joints)
			{
				static_cast<Point &>(joint) = joint.frames.at(size_t(frame));
			}
		}

		for (size_t m = 0; m < model.meshes.size(); m++)
		{
			auto &mesh = model.meshes[m];
			for (size_t v = 0; v < mesh.vertices.size(); v++)
			{
				mesh.vertices[v].pos = positions[m][v];
			}
			calculateNormals(model, mesh, FRAME_BIND_POSE);
		}

		model.hasBindPose = true;
	}

	bool canBeTag(const SkeletalModel &model, int index)
	{
		if (index < 0 || index >= int(model.joints.size()))
		{
			return false;
		}

		auto hasIndex = [index](int value) { return value == index; };
		if (std::any_of(model.joints.begin(), model.joints.end(), [&](const Joint &joint) { return hasIndex(joint.parentId); })
			|| std::any_of(model.tags.begin(), model.tags.end(), [&](const BoneTag &tag) { return hasIndex(tag.parentBone); })
			|| std::any_of(model.hitboxes.begin(), model.hitboxes.end(),
						   [&](const Hitbox &hitbox) { return hasIndex(hitbox.parentJoint); }))
		{
			return false;
		}

		for (const auto &mesh : model.meshes)
		{
			if (std::any_of(mesh.weights.begin(), mesh.weights.end(), [&](const Weight &weight) { return hasIndex(weight.jointId); }))
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * Remap every bone index of the model
	 * @param model model to change
	 * @param remap remap[old index] = new index, -1 for removed bones (nothing may point to them)
	 */
	static void remapJointIndices(SkeletalModel &model, const std::vector<int> &remap)
	{
		auto map = [&](int index) { return index < 0 ? index : remap.at(size_t(index)); };

		for (auto &joint : model.joints)
		{
			joint.parentId = map(joint.parentId);
		}
		for (auto &mesh : model.meshes)
		{
			for (auto &weight : mesh.weights)
			{
				weight.jointId = map(weight.jointId);
			}
		}
		for (auto &tag : model.tags)
		{
			tag.parentBone = map(tag.parentBone);
		}
		for (auto &hitbox : model.hitboxes)
		{
			hitbox.parentJoint = map(hitbox.parentJoint);
		}
		model.torsoParent = map(model.torsoParent);
	}

	int bakeTags(SkeletalModel &model)
	{
		int baked = 0;

		for (int index = int(model.joints.size()) - 1; index >= 0; index--)
		{
			const Joint &joint = model.joints[index];
			if (!joint.asTag || joint.parentId < 0 || !canBeTag(model, index) || model.torsoParent == index)
			{
				continue;
			}

			const bool useBind = model.hasBindPose || joint.frames.empty();
			const Point &pose = useBind ? static_cast<const Point &>(joint) : joint.frames.front();
			const Joint &parentJoint = model.joints[joint.parentId];
			const Point &parent
				= useBind || parentJoint.frames.empty() ? static_cast<const Point &>(parentJoint) : parentJoint.frames.front();

			BoneTag tag;
			tag.name = joint.name;
			tag.parentBone = joint.parentId;
			tag.location = glm::inverse(parent.rotation) * (pose.location - parent.location);
			tag.rotation = glm::normalize(glm::inverse(parent.rotation) * pose.rotation);
			if (model.flipBoneTags)
			{
				tag.rotation = glm::normalize(tag.rotation * BONE_TAG_FLIP);
			}

			std::vector<int> remap(model.joints.size());
			for (int i = 0; i < int(remap.size()); i++)
			{
				remap[i] = i < index ? i : (i == index ? -1 : i - 1);
			}

			model.joints.erase(model.joints.begin() + index);
			remapJointIndices(model, remap);
			tag.parentBone = remap[tag.parentBone];
			model.tags.push_back(tag);
			baked++;
		}

		return baked;
	}

	void reorderJoints(SkeletalModel &model, const std::vector<int> &order)
	{
		const size_t count = model.joints.size();
		if (order.size() != count)
		{
			throw mt_ex("The bone order has " + std::to_string(order.size()) + " bones, the model has " + std::to_string(count));
		}

		std::vector<int> remap(count, -1);
		for (size_t i = 0; i < count; i++)
		{
			if (order[i] < 0 || order[i] >= int(count) || remap[order[i]] >= 0)
			{
				throw mt_ex("The bone order is not a permutation of the bones");
			}
			remap[order[i]] = int(i);
		}

		for (size_t i = 0; i < count; i++)
		{
			int parent = model.joints[order[i]].parentId;
			if (parent >= 0 && remap[parent] >= int(i))
			{
				throw mt_ex("The bone order puts " + model.joints[order[i]].name + " before its parent");
			}
		}

		std::vector<Joint> joints;
		joints.reserve(count);
		for (int old : order)
		{
			joints.push_back(model.joints[old]);
		}
		model.joints = std::move(joints);
		remapJointIndices(model, remap);
	}

	void scaleBounds(Model &model, float scale, BoundsAxes axes)
	{
		glm::vec3 factor{1.0f};
		switch (axes)
		{
		case BoundsAxes::X: factor.x = scale; break;
		case BoundsAxes::Y: factor.y = scale; break;
		case BoundsAxes::Z: factor.z = scale; break;
		case BoundsAxes::XY: factor.x = factor.y = scale; break;
		case BoundsAxes::All: factor = glm::vec3{scale}; break;
		}

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			for (auto &bounds : skeletal->bounds)
			{
				bounds.min *= factor;
				bounds.max *= factor;
			}
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			for (auto &frame : morph->frames)
			{
				frame.bounds.min *= factor;
				frame.bounds.max *= factor;
				frame.localOrigin = (frame.bounds.min + frame.bounds.max) * 0.5f;
				frame.radius = glm::distance(frame.localOrigin, frame.bounds.max);
			}
		}
	}
}
