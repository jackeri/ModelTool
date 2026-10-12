#pragma once

#include "library.h"
#include "model.h"
#include "hitbox.h"

namespace mt::model {

	/*! Which pose of a model to build */
	struct PoseOptions {
		bool bindPose = false;	 ///< use the bind pose (skeletal models with a bind pose only)
		int frame = 0;			 ///< frame to show
		int nextFrame = 0;		 ///< frame to blend towards
		float blend = 0.0f;		 ///< 0 = frame, 1 = nextFrame
		int lodLevel = 0;		 ///< level of detail (morph models with extra levels)
		bool dynamicLod = false; ///< reduce the triangles with the collapse maps
		float lodRatio = 0.0f;	 ///< dynamic level of detail: 0 = the mesh's minimum LOD vertex count, 1 = all vertices
	};

	/*! Mesh in object space, ready to be drawn */
	struct PosedMesh {
		std::string name{};
		std::string material{};
		std::vector<glm::vec3> positions{};
		std::vector<glm::vec3> normals{};
		std::vector<glm::vec2> texCoords{};
		std::vector<glm::ivec3> tris{};
	};

	/*! Tag (or bone used as a tag) in object space */
	struct PosedTag {
		std::string name{};
		Point point{};
	};

	/*! Hitbox with its object space transform */
	struct PosedHitbox {
		Point point{};	 ///< object space location and rotation
		Hitbox hitbox{}; ///< the hitbox (shape and size)
	};

	/*! Everything that can be drawn of a model in one pose */
	struct PosedModel {
		std::vector<PosedMesh> meshes{};
		std::vector<glm::vec3> joints{};					  ///< bone positions (tags excluded)
		std::vector<std::pair<glm::vec3, glm::vec3>> bones{}; ///< lines from bones to their parents (tags excluded)
		std::vector<PosedTag> tags{};
		std::vector<PosedHitbox> hitboxes{};
		bool hasBounds = false;
		Bounds bounds{};
	};

	/**
	 * Build the drawable data of a model in a pose
	 * @param model model to pose
	 * @param options pose to build
	 * @return posed model, empty meshes when there is nothing to show (skeletal model without frames or bind pose)
	 */
	PosedModel poseModel(const Model &model, const PoseOptions &options);

	/**
	 * Remap triangles with a collapse map (Melax progressive mesh) so that only the first vertexCount vertices are used,
	 * dropping triangles that collapse to a line or a point
	 * @param tris triangles of the mesh
	 * @param collapseMap collapse target of every vertex, empty for no reduction
	 * @param vertexCount number of vertices to keep
	 * @return reduced triangles
	 */
	std::vector<glm::ivec3> lodTriangles(const std::vector<glm::ivec3> &tris, const std::vector<int> &collapseMap, int vertexCount);

	/**
	 * Vertex count for dynamic level of detail
	 * @param numVertices vertices in the mesh
	 * @param minLod minimum vertex count of the mesh
	 * @param ratio 0 = minLod, 1 = all vertices
	 * @return vertex count to keep
	 */
	int lodVertexCount(int numVertices, int minLod, float ratio);
}
