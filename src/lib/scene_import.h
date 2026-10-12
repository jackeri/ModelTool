#pragma once

#include "library.h"
#include "model.h"

/*
 * Intermediate scene used by the general purpose importers (FBX / OBJ through ufbx, glTF through tinygltf). The
 * importers fill a Scene and toModel() turns it into a skeletal model (skinned scenes) or a morph model (static scenes).
 */
namespace mt::model::import {

	/*! Scene node, a bone or a tag */
	struct Node {
		std::string name{};
		int parent = -1;				 ///< parent node, needs to come before this node
		glm::mat4 bind{1.0f};			 ///< world transform in the bind pose
		std::vector<glm::mat4> frames{}; ///< world transform per frame
	};

	/*! Bone influence of a vertex */
	struct Influence {
		int node{};		///< bone node
		float weight{}; ///< weight, the weights of a vertex are normalized by toModel
	};

	/*! Mesh vertex in the bind pose (world space) */
	struct Vertex {
		glm::vec3 pos{};
		glm::vec3 normal{};
		glm::vec2 tex{};
		std::vector<Influence> influences{};
	};

	/*! Mesh with triangles in idTech (clockwise) winding */
	struct Mesh {
		std::string name{};
		std::string material{};
		std::vector<Vertex> vertices{};
		std::vector<glm::ivec3> tris{};
		std::vector<std::vector<glm::vec3>> framePositions{}; ///< static scenes: world space positions per frame
		std::vector<std::vector<glm::vec3>> frameNormals{};	  ///< static scenes: world space normals per frame
	};

	/*! Animation sequence, a range of frames */
	struct Animation {
		std::string name{};
		int startFrame{};
		int numFrames{};
		int fps{};
	};

	/*! Imported scene */
	struct Scene {
		std::string name{};
		bool skinned = false;	   ///< true if any mesh is bound to bones, gives a skeletal model
		std::vector<Node> nodes{}; ///< bones (skinned) and tags
		std::vector<int> tags{};   ///< nodes that are tags (named tag_*)
		std::vector<Mesh> meshes{};
		std::vector<Animation> animations{};
		int numFrames = 0;				///< frames in Node::frames and Mesh::framePositions
		glm::mat3 axisConversion{1.0f}; ///< rotation applied to the whole scene (to Z-up)
		float unitScale = 1.0f;			///< scale applied to the whole scene (to game units)
	};

	/**
	 * Convert an imported scene into a model
	 * @param scene scene to convert
	 * @return skeletal model for skinned scenes, morph model for static scenes, owned by the caller
	 * @throws mt::mt_exception if the scene is inconsistent
	 */
	Model *toModel(Scene &&scene);

	/**
	 * @return true if the node name marks a tag ("tag_" prefix, case insensitive)
	 */
	bool isTagName(const std::string &name);

	/**
	 * Default sampling rate for animations that do not define one
	 */
	const int DEFAULT_FPS = 24;
}
