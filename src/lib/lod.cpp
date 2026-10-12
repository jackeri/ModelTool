#include "lod.h"
#include "skeletal_tools.h"

#include <list>

namespace mt::model {

	namespace {
		/*! Working data of the reduction, everything is referenced by index */
		class Reducer {
		  public:
			Reducer(const std::vector<glm::vec3> &positions, const std::vector<glm::ivec3> &tris)
			{
				vertices.resize(positions.size());
				for (size_t i = 0; i < positions.size(); i++)
				{
					vertices[i].position = positions[i];
				}

				for (const auto &tri : tris)
				{
					if (tri.x == tri.y || tri.y == tri.z || tri.z == tri.x)
					{
						continue; // degenerate triangles have no normal and no edges to collapse
					}

					const int index = int(triangles.size());
					triangles.push_back({{tri.x, tri.y, tri.z}, {}, true});
					computeNormal(index);
					for (int corner = 0; corner < 3; corner++)
					{
						Vertex &vert = vertices[tri[corner]];
						vert.faces.push_back(index);
						for (int other = 0; other < 3; other++)
						{
							if (other != corner)
							{
								addUnique(vert.neighbors, tri[other]);
							}
						}
					}
				}
			}

			ProgressiveMesh run()
			{
				const int count = int(vertices.size());
				ProgressiveMesh out;
				out.permutation.assign(size_t(count), 0);
				out.collapseMap.assign(size_t(count), 0);
				std::vector<int> collapseTo(size_t(count), -1);
				out.minLod = -1;

				for (int v = 0; v < count; v++)
				{
					computeCost(v);
				}

				for (int remaining = count; remaining > 0; remaining--)
				{
					int vertex = minimumCostVertex(remaining, out.minLod);
					out.permutation[vertex] = remaining - 1;
					collapseTo[size_t(remaining - 1)] = vertices[vertex].collapse;
					collapse(vertex, vertices[vertex].collapse);
				}

				// The collapse targets in the new order
				for (int i = 0; i < count; i++)
				{
					out.collapseMap[i] = collapseTo[i] < 0 ? 0 : out.permutation[collapseTo[i]];
				}

				if (out.minLod < 0)
				{
					out.minLod = 0;
				}
				return out;
			}

		  private:
			struct Vertex {
				glm::vec3 position{};
				std::vector<int> neighbors{};
				std::vector<int> faces{};
				float cost = 0;
				int collapse = -1; ///< vertex this one is cheapest to collapse into
				bool removed = false;
			};

			struct Triangle {
				int vertex[3];
				glm::vec3 normal;
				bool alive;

				[[nodiscard]] bool has(int v) const
				{
					return vertex[0] == v || vertex[1] == v || vertex[2] == v;
				}
			};

			static void addUnique(std::vector<int> &list, int value)
			{
				if (std::find(list.begin(), list.end(), value) == list.end())
				{
					list.push_back(value);
				}
			}

			static void remove(std::vector<int> &list, int value)
			{
				list.erase(std::remove(list.begin(), list.end(), value), list.end());
			}

			void computeNormal(int index)
			{
				Triangle &tri = triangles[index];
				const glm::vec3 &a = vertices[tri.vertex[0]].position;
				const glm::vec3 &b = vertices[tri.vertex[1]].position;
				const glm::vec3 &c = vertices[tri.vertex[2]].position;
				glm::vec3 normal = glm::cross(b - a, c - b);
				float length = glm::length(normal);
				tri.normal = length > 0 ? normal / length : glm::vec3{0};
			}

			int trianglesOnEdge(int u, int v) const
			{
				int count = 0;
				for (int face : vertices[u].faces)
				{
					count += triangles[face].has(v) ? 1 : 0;
				}
				return count;
			}

			/*! A vertex on an edge that has only one triangle (a border or a texture seam) */
			bool onSilhouette(int u) const
			{
				for (int face : vertices[u].faces)
				{
					for (int other : triangles[face].vertex)
					{
						if (other != u && trianglesOnEdge(u, other) == 1)
						{
							return true;
						}
					}
				}
				return false;
			}

			float edgeCost(int u, int v) const
			{
				const float length = glm::distance(vertices[v].position, vertices[u].position);

				std::vector<int> sides;
				for (int face : vertices[u].faces)
				{
					if (triangles[face].has(v))
					{
						sides.push_back(face);
					}
				}

				// The more coplanar the faces around u are, the cheaper the collapse
				float curvature = 0;
				for (int face : vertices[u].faces)
				{
					float minCurvature = 1;
					for (int side : sides)
					{
						float dot = glm::dot(triangles[face].normal, triangles[side].normal);
						minCurvature = std::min(minCurvature, (1 - dot) / 2.0f);
					}
					curvature = std::max(curvature, minCurvature);
				}

				return length * curvature + (onSilhouette(u) ? 100000.0f : 0.0f);
			}

			void computeCost(int v)
			{
				Vertex &vert = vertices[v];
				if (vert.neighbors.empty())
				{
					vert.collapse = -1;
					vert.cost = -0.01f;
					return;
				}

				vert.cost = 1000000;
				vert.collapse = -1;
				for (int neighbor : vert.neighbors)
				{
					float cost = edgeCost(v, neighbor);
					if (cost < vert.cost)
					{
						vert.collapse = neighbor;
						vert.cost = cost;
					}
				}
			}

			int minimumCostVertex(int remaining, int &minLod) const
			{
				// Lone vertices go first
				for (size_t v = 0; v < vertices.size(); v++)
				{
					if (!vertices[v].removed && vertices[v].collapse < 0)
					{
						return int(v);
					}
				}

				int best = -1;
				for (size_t v = 0; v < vertices.size(); v++)
				{
					if (vertices[v].removed || (best >= 0 && vertices[v].cost >= vertices[best].cost))
					{
						continue;
					}
					if (!onSilhouette(int(v)))
					{
						best = int(v);
					}
				}

				if (best >= 0)
				{
					return best;
				}

				// Only silhouette vertices are left, the minimum LOD is reached
				if (minLod < 0)
				{
					minLod = remaining;
				}

				for (size_t v = 0; v < vertices.size(); v++)
				{
					if (!vertices[v].removed && (best < 0 || vertices[v].cost < vertices[best].cost))
					{
						best = int(v);
					}
				}
				return best;
			}

			void collapse(int u, int v)
			{
				Vertex &from = vertices[u];
				std::vector<int> neighbors = from.neighbors;

				if (v >= 0)
				{
					// Remove the triangles on the edge, move the others from u to v
					for (int face : std::vector<int>(from.faces))
					{
						Triangle &tri = triangles[face];
						if (tri.has(v))
						{
							removeTriangle(face);
						}
					}

					for (int face : std::vector<int>(from.faces))
					{
						replaceVertex(face, u, v);
					}
				}

				// Detach u from its neighbors
				for (int neighbor : neighbors)
				{
					remove(vertices[neighbor].neighbors, u);
				}
				from.neighbors.clear();
				from.faces.clear();
				from.removed = true;

				for (int neighbor : neighbors)
				{
					computeCost(neighbor);
				}
			}

			void removeTriangle(int face)
			{
				Triangle &tri = triangles[face];
				tri.alive = false;
				for (int corner = 0; corner < 3; corner++)
				{
					remove(vertices[tri.vertex[corner]].faces, face);
				}

				// Vertices stay neighbors only if another triangle still connects them
				for (int a = 0; a < 3; a++)
				{
					for (int b = 0; b < 3; b++)
					{
						int va = tri.vertex[a], vb = tri.vertex[b];
						if (a != b && trianglesOnEdge(va, vb) == 0)
						{
							remove(vertices[va].neighbors, vb);
						}
					}
				}
			}

			void replaceVertex(int face, int from, int to)
			{
				Triangle &tri = triangles[face];
				for (int &corner : tri.vertex)
				{
					if (corner == from)
					{
						corner = to;
					}
				}
				remove(vertices[from].faces, face);
				addUnique(vertices[to].faces, face);

				for (int corner : tri.vertex)
				{
					if (corner == to)
					{
						continue;
					}
					// u's old neighbors that were only connected through this triangle
					if (trianglesOnEdge(from, corner) == 0)
					{
						remove(vertices[from].neighbors, corner);
						remove(vertices[corner].neighbors, from);
					}
					addUnique(vertices[corner].neighbors, to);
					addUnique(vertices[to].neighbors, corner);
				}

				computeNormal(face);
			}

			std::vector<Vertex> vertices{};
			std::vector<Triangle> triangles{};
		};

		template<typename T>
		std::vector<T> permute(const std::vector<T> &values, const std::vector<int> &permutation)
		{
			std::vector<T> out(values.size());
			for (size_t i = 0; i < values.size(); i++)
			{
				out[size_t(permutation[i])] = values[i];
			}
			return out;
		}

		void remapTriangles(std::vector<glm::ivec3> &tris, const std::vector<int> &permutation)
		{
			for (auto &tri : tris)
			{
				for (int i = 0; i < 3; i++)
				{
					tri[i] = permutation.at(size_t(tri[i]));
				}
			}
		}
	}

	ProgressiveMesh progressiveMesh(const std::vector<glm::vec3> &positions, const std::vector<glm::ivec3> &tris)
	{
		for (const auto &tri : tris)
		{
			for (int i = 0; i < 3; i++)
			{
				if (tri[i] < 0 || tri[i] >= int(positions.size()))
				{
					throw mt_ex("Triangle points to an invalid vertex: " + std::to_string(tri[i]));
				}
			}
		}

		return Reducer(positions, tris).run();
	}

	void calculateLod(skeletal::SkeletalModel &model, int frame)
	{
		for (auto &mesh : model.meshes)
		{
			std::vector<glm::vec3> positions;
			for (const auto &vert : mesh.vertices)
			{
				positions.push_back(skeletal::posedVertex(model, mesh, vert, frame));
			}

			ProgressiveMesh result = progressiveMesh(positions, mesh.tris);
			mesh.vertices = permute(mesh.vertices, result.permutation);
			remapTriangles(mesh.tris, result.permutation);
			mesh.collapseMap = result.collapseMap;
			mesh.minLod = result.minLod;
		}
	}

	void calculateLod(morph::MorphModel &model, int frame)
	{
		frame = std::clamp(frame, 0, std::max(model.numFrames() - 1, 0));

		for (auto &mesh : model.meshes)
		{
			if (mesh.frames.empty())
			{
				continue;
			}

			std::vector<glm::vec3> positions;
			for (const auto &vert : mesh.frames[size_t(frame)])
			{
				positions.push_back(vert.pos);
			}

			ProgressiveMesh result = progressiveMesh(positions, mesh.tris);
			mesh.texCoords = permute(mesh.texCoords, result.permutation);
			for (auto &frameVertices : mesh.frames)
			{
				frameVertices = permute(frameVertices, result.permutation);
			}
			remapTriangles(mesh.tris, result.permutation);
			mesh.collapseMap = result.collapseMap;
			mesh.minLod = result.minLod;
		}
	}

	bool calculateLod(Model &model)
	{
		if (auto *skeletal = dynamic_cast<skeletal::SkeletalModel *>(&model))
		{
			int frame = model.currentFrame;
			if (frame == FRAME_BIND_POSE || !model.hasFrames())
			{
				if (!skeletal->hasBindPose)
				{
					return false;
				}
				frame = FRAME_BIND_POSE;
			}
			calculateLod(*skeletal, std::min(frame, model.numFrames() - 1));
			return true;
		}

		if (auto *morph = dynamic_cast<morph::MorphModel *>(&model))
		{
			calculateLod(*morph, model.currentFrame);
			return true;
		}

		return false;
	}
}
