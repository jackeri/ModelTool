#pragma once

#include "library.h"
#include "model.h"

namespace mt::model::morph {

	/*! Vertex position and normal in a single frame */
	struct FrameVertex {
		glm::vec3 pos{};	///< position vector
		glm::vec3 normal{}; ///< normal vector
	};

	/*! Frame information shared by all the meshes */
	struct Frame {
		std::string name{};		 ///< name of the frame
		Bounds bounds{};		 ///< bounds of all the meshes in this frame
		glm::vec3 localOrigin{}; ///< midpoint of the bounds
		float radius{};			 ///< distance from the local origin to a corner of the bounds
	};

	/*! Tag with a location and rotation for every frame */
	struct Tag {
		std::string name{};		   ///< name of the tag
		std::vector<Point> frames; ///< tag location and rotation per frame
	};

	/*! Morph models mesh, every frame stores a full copy of the vertex positions and normals */
	struct Mesh {
		std::string name{};								///< name of the mesh
		std::string materialName{};						///< name of the material for the mesh
		int flags{};									///< format specific surface flags
		std::vector<glm::vec2> texCoords{};				///< texture coordinates, shared by all frames (one per vertex)
		std::vector<std::vector<FrameVertex>> frames{}; ///< vertices per frame, frames[frame][vertex]
		std::vector<glm::ivec3> tris{};					///< mesh triangles
		int minLod = 0;									///< lodding level of the mesh
		std::vector<int> collapseMap{};					///< collapse map for dynamic lodding (level of detail)
		reshandle material{};							///< material instance used in this mesh

		/**
		 * @return number of vertices in the mesh
		 */
		[[nodiscard]] size_t numVertices() const
		{
			return texCoords.size();
		}
	};

	/*! Model which stores the vertices of every frame (no skeleton), like MD3 and MDC */
	struct MorphModel : public Model {
		std::vector<Frame> frames{};		   ///< frame information
		std::vector<Mesh> meshes{};			   ///< meshes of the base level of detail
		std::vector<Tag> tags{};			   ///< tags, every tag has a location for every frame
		std::vector<std::vector<Mesh>> lods{}; ///< extra levels of detail (LOD 1 and up), LOD 0 is meshes

		MorphModel()
		{
			// Morph models have no bind pose so start from the first frame
			currentFrame = 0;
		}

		int numFrames() const override
		{
			return int(frames.size());
		}

		bool morphModel() const override
		{
			return true;
		}

		bool hasTags() const override
		{
			return !tags.empty();
		}

		/**
		 * @return number of levels of detail including the base level
		 */
		[[nodiscard]] int lodCount() const
		{
			return int(lods.size()) + 1;
		}

		/**
		 * Get the meshes of a level of detail
		 * @param level 0 for the base meshes, 1 and up for the extra levels
		 * @return meshes of the level
		 */
		std::vector<Mesh> &lod(int level);

		/**
		 * @return the current frame clamped to a valid frame index
		 */
		[[nodiscard]] int frameIndex() const;

		/**
		 * Check that every mesh and tag has data for every frame and that the triangles point to valid vertices
		 * @throws mt::mt_exception if the data is inconsistent
		 */
		void validate() const;
	};
}
