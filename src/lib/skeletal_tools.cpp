#include "skeletal_tools.h"

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
}
