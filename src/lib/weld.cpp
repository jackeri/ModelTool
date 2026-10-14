#include "weld.h"
#include "morph_model.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"

#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace mt::model {

	namespace {
		/*! A vertex of a mesh */
		struct VertexRef {
			size_t mesh;
			size_t vertex;
		};

		/**
		 * Group points that are closer than the distance: every point not in a group yet starts one with the points near
		 * it that are not in a group either (in order, so the result does not depend on hash order)
		 * @param points points to group
		 * @param distance largest distance to the first point of the group
		 * @param scopes points of different scopes are not grouped (the mesh of each point), empty for one scope
		 * @return groups of two or more points
		 */
		std::vector<std::vector<size_t>>
		groupPoints(const std::vector<glm::vec3> &points, float distance, const std::vector<size_t> &scopes)
		{
			using Cell = std::tuple<int, int, int>;
			const float cellSize = std::max(distance, 1e-6f);
			auto cellOf = [&](const glm::vec3 &pos) {
				const glm::ivec3 cell = glm::ivec3(glm::floor(pos / cellSize));
				return Cell{cell.x, cell.y, cell.z};
			};
			std::map<Cell, std::vector<size_t>> cells;
			for (size_t i = 0; i < points.size(); i++)
			{
				cells[cellOf(points[i])].push_back(i);
			}

			std::vector<bool> used(points.size(), false);
			std::vector<std::vector<size_t>> groups;
			for (size_t i = 0; i < points.size(); i++)
			{
				if (used[i])
				{
					continue;
				}
				used[i] = true;
				std::vector<size_t> group{i};
				const auto [cx, cy, cz] = cellOf(points[i]);
				for (int dx = -1; dx <= 1; dx++)
				{
					for (int dy = -1; dy <= 1; dy++)
					{
						for (int dz = -1; dz <= 1; dz++)
						{
							auto found = cells.find({cx + dx, cy + dy, cz + dz});
							if (found == cells.end())
							{
								continue;
							}
							for (size_t other : found->second)
							{
								if (!used[other] && (scopes.empty() || scopes[other] == scopes[i])
									&& glm::distance(points[i], points[other]) <= distance)
								{
									used[other] = true;
									group.push_back(other);
								}
							}
						}
					}
				}
				if (group.size() > 1)
				{
					std::sort(group.begin(), group.end());
					groups.push_back(std::move(group));
				}
			}
			return groups;
		}

		WeldResult weldMorphMeshes(std::vector<morph::Mesh> &meshes, int frame, float distance, bool acrossMeshes)
		{
			std::vector<VertexRef> refs;
			std::vector<glm::vec3> points;
			std::vector<size_t> scopes;
			for (size_t m = 0; m < meshes.size(); m++)
			{
				if (meshes[m].frames.empty())
				{
					continue;
				}
				const auto &vertices = meshes[m].frames[size_t(std::clamp(frame, 0, int(meshes[m].frames.size()) - 1))];
				for (size_t v = 0; v < vertices.size(); v++)
				{
					refs.push_back({m, v});
					points.push_back(vertices[v].pos);
					scopes.push_back(acrossMeshes ? 0 : m);
				}
			}

			WeldResult result;
			for (const auto &group : groupPoints(points, distance, scopes))
			{
				result.groups++;
				glm::vec3 average{0.0f};
				for (size_t i : group)
				{
					average += points[i];
				}
				average /= float(group.size());
				for (size_t i : group)
				{
					result.moved += points[i] != average ? 1 : 0;
				}

				// Every frame: the members move to their average place in that frame
				const size_t frames = meshes[refs[group[0]].mesh].frames.size();
				for (size_t f = 0; f < frames; f++)
				{
					glm::vec3 place{0.0f};
					for (size_t i : group)
					{
						place += meshes[refs[i].mesh].frames[f][refs[i].vertex].pos;
					}
					place /= float(group.size());
					for (size_t i : group)
					{
						meshes[refs[i].mesh].frames[f][refs[i].vertex].pos = place;
					}
				}
			}
			return result;
		}

		WeldResult weldSkeletal(skeletal::SkeletalModel &model, float distance, bool acrossMeshes)
		{
			const int frame
				= model.hasBindPose || !model.hasFrames() ? FRAME_BIND_POSE : std::clamp(model.currentFrame, 0, model.numFrames() - 1);

			std::vector<VertexRef> refs;
			std::vector<glm::vec3> points;
			std::vector<size_t> scopes;
			for (size_t m = 0; m < model.meshes.size(); m++)
			{
				const auto &mesh = model.meshes[m];
				for (size_t v = 0; v < mesh.vertices.size(); v++)
				{
					refs.push_back({m, v});
					points.push_back(skeletal::posedVertex(model, mesh, mesh.vertices[v], frame));
					scopes.push_back(acrossMeshes ? 0 : m);
				}
			}

			// The vertex whose weights each vertex takes: its own, or the first one of its group
			std::vector<std::vector<VertexRef>> source(model.meshes.size());
			for (size_t m = 0; m < model.meshes.size(); m++)
			{
				for (size_t v = 0; v < model.meshes[m].vertices.size(); v++)
				{
					source[m].push_back({m, v});
				}
			}

			WeldResult result;
			for (const auto &group : groupPoints(points, distance, scopes))
			{
				result.groups++;
				const VertexRef first = refs[group[0]];
				for (size_t i : group)
				{
					source[refs[i].mesh][refs[i].vertex] = first;
					result.moved += points[i] != points[group[0]] ? 1 : 0;
				}
			}
			if (result.groups == 0)
			{
				return result;
			}

			// New weight lists, copied from the original meshes
			const std::vector<skeletal::Mesh> original = model.meshes;
			for (size_t m = 0; m < model.meshes.size(); m++)
			{
				auto &mesh = model.meshes[m];
				std::vector<skeletal::Weight> weights;
				for (size_t v = 0; v < mesh.vertices.size(); v++)
				{
					const VertexRef from = source[m][v];
					const auto &fromMesh = original[from.mesh];
					const auto &fromVertex = fromMesh.vertices[from.vertex];
					auto &vertex = mesh.vertices[v];

					vertex.pos = fromVertex.pos;
					const int start = int(weights.size());
					for (int w = 0; w < fromVertex.weightCount; w++)
					{
						const size_t index = size_t(fromVertex.startWeight + w);
						if (index >= fromMesh.weights.size())
						{
							throw mt_ex("Vertex points to an invalid weight: " + std::to_string(index));
						}
						weights.push_back(fromMesh.weights[index]);
					}
					if (from.mesh != m || from.vertex != v)
					{
						const auto &own = original[m];
						bool same = own.vertices[v].weightCount == fromVertex.weightCount;
						for (int w = 0; same && w < fromVertex.weightCount; w++)
						{
							const size_t index = size_t(own.vertices[v].startWeight + w);
							same = index < own.weights.size() && own.weights[index] == weights[size_t(start + w)];
						}
						result.reweighted += same ? 0 : 1;
					}
					vertex.startWeight = start;
					vertex.weightCount = fromVertex.weightCount;
				}
				mesh.weights = std::move(weights);
			}
			return result;
		}
	}

	WeldResult weldVertices(Model &model, float distance, bool acrossMeshes)
	{
		if (distance < 0.0f)
		{
			throw mt_ex("The weld distance can not be negative");
		}

		if (auto *skeletal = dynamic_cast<skeletal::SkeletalModel *>(&model))
		{
			return weldSkeletal(*skeletal, distance, acrossMeshes);
		}

		WeldResult result;
		if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			const int frame = std::max(model.currentFrame, 0);
			result = weldMorphMeshes(morph->meshes, frame, distance, acrossMeshes);
			for (auto &level : morph->lods)
			{
				const WeldResult lod = weldMorphMeshes(level, frame, distance, acrossMeshes);
				result.groups += lod.groups;
				result.moved += lod.moved;
			}
		}
		return result;
	}
}
