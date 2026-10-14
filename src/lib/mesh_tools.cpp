#include "mesh_tools.h"
#include "morph_model.h"
#include "skeletal_model.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace mt::model {
	using morph::MorphModel;
	using skeletal::SkeletalModel;

	namespace {
		bool sameName(const std::string &a, const std::string &b)
		{
			return tools::lowercase(a) == tools::lowercase(b);
		}

		/*! Call fn for a morph mesh and the meshes with its name in the levels of detail, or for every mesh */
		void forMorphMeshes(MorphModel &model, int mesh, const std::function<void(morph::Mesh &)> &fn)
		{
			if (mesh >= int(model.meshes.size()))
			{
				throw mt_ex("Invalid mesh: " + std::to_string(mesh));
			}
			const std::string name = mesh >= 0 ? model.meshes[size_t(mesh)].name : std::string();
			for (size_t i = 0; i < model.meshes.size(); i++)
			{
				if (mesh < 0 || int(i) == mesh)
				{
					fn(model.meshes[i]);
				}
			}
			for (auto &level : model.lods)
			{
				for (auto &lodMesh : level)
				{
					if (mesh < 0 || sameName(lodMesh.name, name))
					{
						fn(lodMesh);
					}
				}
			}
		}

		void forSkeletalMeshes(SkeletalModel &model, int mesh, const std::function<void(skeletal::Mesh &)> &fn)
		{
			if (mesh >= int(model.meshes.size()))
			{
				throw mt_ex("Invalid mesh: " + std::to_string(mesh));
			}
			for (size_t i = 0; i < model.meshes.size(); i++)
			{
				if (mesh < 0 || int(i) == mesh)
				{
					fn(model.meshes[i]);
				}
			}
		}

		/*! Morph or skeletal meshes, else an error */
		void forMeshes(Model &model, int mesh, const std::function<void(morph::Mesh &)> &morphFn,
					   const std::function<void(skeletal::Mesh &)> &skeletalFn)
		{
			if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
			{
				forSkeletalMeshes(*skeletal, mesh, skeletalFn);
			}
			else if (auto *morph = dynamic_cast<MorphModel *>(&model))
			{
				forMorphMeshes(*morph, mesh, morphFn);
			}
			else
			{
				throw mt_ex("The model has no meshes to change");
			}
		}

		glm::vec3 mirrored(glm::vec3 vec, int axis)
		{
			vec[axis] = -vec[axis];
			return vec;
		}

		/*! A location and rotation seen in the mirror: the rotation stays a rotation (S R S) */
		Point mirrored(const Point &point, int axis)
		{
			glm::mat3 flip(1.0f);
			flip[axis][axis] = -1.0f;
			const glm::mat3 rotation = flip * glm::mat3_cast(point.rotation) * flip;
			return {mirrored(point.location, axis), glm::normalize(glm::quat_cast(rotation))};
		}

		void mirrorPoint(Point &point, int axis)
		{
			const Point result = mirrored(point, axis);
			point.location = result.location;
			point.rotation = result.rotation;
		}

		void mirrorBounds(Bounds &bounds, int axis)
		{
			const float low = -bounds.max[axis];
			bounds.max[axis] = -bounds.min[axis];
			bounds.min[axis] = low;
		}

		/*! Append the second mesh to the first (same frame count) */
		void appendMesh(morph::Mesh &into, const morph::Mesh &from)
		{
			const int offset = int(into.numVertices());
			into.texCoords.insert(into.texCoords.end(), from.texCoords.begin(), from.texCoords.end());
			for (size_t f = 0; f < into.frames.size(); f++)
			{
				into.frames[f].insert(into.frames[f].end(), from.frames[f].begin(), from.frames[f].end());
			}
			for (const auto &tri : from.tris)
			{
				into.tris.push_back(tri + offset);
			}
			into.collapseMap.clear();
			into.minLod = 0;
		}

		void appendMesh(skeletal::Mesh &into, const skeletal::Mesh &from)
		{
			const int vertexOffset = int(into.vertices.size());
			const int weightOffset = int(into.weights.size());
			for (auto vertex : from.vertices)
			{
				vertex.startWeight += weightOffset;
				into.vertices.push_back(vertex);
			}
			into.weights.insert(into.weights.end(), from.weights.begin(), from.weights.end());
			for (const auto &tri : from.tris)
			{
				into.tris.push_back(tri + vertexOffset);
			}
			std::set<int> bones(into.boneReferences.begin(), into.boneReferences.end());
			bones.insert(from.boneReferences.begin(), from.boneReferences.end());
			into.boneReferences.assign(bones.begin(), bones.end());
			into.collapseMap.clear();
			into.minLod = 0;
		}

		size_t vertexCount(const morph::Mesh &mesh)
		{
			return mesh.numVertices();
		}

		size_t vertexCount(const skeletal::Mesh &mesh)
		{
			return mesh.vertices.size();
		}

		template<typename MeshType>
		size_t mergeList(std::vector<MeshType> &meshes, size_t maxVertices, size_t maxTriangles)
		{
			std::vector<bool> removed(meshes.size(), false);
			size_t merged = 0;
			for (size_t i = 0; i < meshes.size(); i++)
			{
				if (removed[i] || meshes[i].materialName.empty())
				{
					continue;
				}
				for (size_t j = i + 1; j < meshes.size(); j++)
				{
					if (removed[j] || !sameName(meshes[i].materialName, meshes[j].materialName))
					{
						continue;
					}
					if (vertexCount(meshes[i]) + vertexCount(meshes[j]) > maxVertices
						|| meshes[i].tris.size() + meshes[j].tris.size() > maxTriangles)
					{
						continue;
					}
					appendMesh(meshes[i], meshes[j]);
					removed[j] = true;
					merged++;
				}
			}

			std::vector<MeshType> kept;
			for (size_t i = 0; i < meshes.size(); i++)
			{
				if (!removed[i])
				{
					kept.push_back(std::move(meshes[i]));
				}
			}
			meshes = std::move(kept);
			return merged;
		}
	}

	size_t meshCount(const Model &model)
	{
		if (const auto *skeletal = dynamic_cast<const SkeletalModel *>(&model))
		{
			return skeletal->meshes.size();
		}
		if (const auto *morph = dynamic_cast<const MorphModel *>(&model))
		{
			return morph->meshes.size();
		}
		return 0;
	}

	void removeMesh(Model &model, size_t mesh)
	{
		if (mesh >= meshCount(model))
		{
			throw mt_ex("Invalid mesh: " + std::to_string(mesh));
		}
		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			skeletal->meshes.erase(skeletal->meshes.begin() + long(mesh));
		}
		else if (auto *morph = dynamic_cast<MorphModel *>(&model))
		{
			const std::string name = morph->meshes[mesh].name;
			morph->meshes.erase(morph->meshes.begin() + long(mesh));
			for (auto &level : morph->lods)
			{
				level.erase(
					std::remove_if(level.begin(), level.end(), [&](const morph::Mesh &lodMesh) { return sameName(lodMesh.name, name); }),
					level.end());
			}
		}
	}

	size_t mergeMeshesByMaterial(Model &model, size_t maxVertices, size_t maxTriangles)
	{
		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			return mergeList(skeletal->meshes, maxVertices, maxTriangles);
		}
		size_t merged = 0;
		if (auto *morph = dynamic_cast<MorphModel *>(&model))
		{
			merged = mergeList(morph->meshes, maxVertices, maxTriangles);
			for (auto &level : morph->lods)
			{
				mergeList(level, maxVertices, maxTriangles);
			}
		}
		return merged;
	}

	void mirrorModel(Model &model, int axis)
	{
		if (axis < 0 || axis > 2)
		{
			throw mt_ex("Invalid mirror axis: " + std::to_string(axis));
		}

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			// Joints and frames are in model space, weights and tags in bone space: mirroring each keeps them together
			for (auto &joint : skeletal->joints)
			{
				mirrorPoint(joint, axis);
				for (auto &frame : joint.frames)
				{
					mirrorPoint(frame, axis);
				}
			}
			for (auto &mesh : skeletal->meshes)
			{
				for (auto &vertex : mesh.vertices)
				{
					vertex.pos = mirrored(vertex.pos, axis);
					vertex.normal = mirrored(vertex.normal, axis);
					vertex.bindNormal = mirrored(vertex.bindNormal, axis);
				}
				for (auto &weight : mesh.weights)
				{
					weight.pos = mirrored(weight.pos, axis);
				}
			}
			for (auto &tag : skeletal->tags)
			{
				mirrorPoint(tag, axis);
			}
			for (auto &hitbox : skeletal->hitboxes)
			{
				mirrorPoint(hitbox, axis);
				mirrorBounds(hitbox, axis);
			}
			for (auto &bounds : skeletal->bounds)
			{
				mirrorBounds(bounds, axis);
			}
		}
		else if (auto *morph = dynamic_cast<MorphModel *>(&model))
		{
			forMorphMeshes(*morph, -1, [&](morph::Mesh &mesh) {
				for (auto &frame : mesh.frames)
				{
					for (auto &vertex : frame)
					{
						vertex.pos = mirrored(vertex.pos, axis);
						vertex.normal = mirrored(vertex.normal, axis);
					}
				}
			});
			for (auto &tag : morph->tags)
			{
				for (auto &frame : tag.frames)
				{
					mirrorPoint(frame, axis);
				}
			}
			for (auto &frame : morph->frames)
			{
				mirrorBounds(frame.bounds, axis);
				frame.localOrigin = mirrored(frame.localOrigin, axis);
			}
		}
		else
		{
			throw mt_ex("The model has no meshes to change");
		}

		// A mirror turns the triangles inside out
		reverseWinding(model, -1);
	}

	void flipNormals(Model &model, int mesh)
	{
		forMeshes(
			model, mesh,
			[](morph::Mesh &target) {
				for (auto &frame : target.frames)
				{
					for (auto &vertex : frame)
					{
						vertex.normal = -vertex.normal;
					}
				}
			},
			[](skeletal::Mesh &target) {
				for (auto &vertex : target.vertices)
				{
					vertex.normal = -vertex.normal;
					vertex.bindNormal = -vertex.bindNormal;
				}
			});
	}

	void reverseWinding(Model &model, int mesh)
	{
		auto reverse = [](std::vector<glm::ivec3> &tris) {
			for (auto &tri : tris)
			{
				std::swap(tri.y, tri.z);
			}
		};
		forMeshes(model, mesh, [&](morph::Mesh &target) { reverse(target.tris); }, [&](skeletal::Mesh &target) { reverse(target.tris); });
	}

	void flipTexCoords(Model &model, int mesh, bool u, bool v)
	{
		auto flip = [&](glm::vec2 &tex) {
			if (u)
			{
				tex.x = 1.0f - tex.x;
			}
			if (v)
			{
				tex.y = 1.0f - tex.y;
			}
		};
		forMeshes(
			model, mesh,
			[&](morph::Mesh &target) {
				for (auto &tex : target.texCoords)
				{
					flip(tex);
				}
			},
			[&](skeletal::Mesh &target) {
				for (auto &vertex : target.vertices)
				{
					flip(vertex.tex);
				}
			});
	}
}
