#include "morph_model.h"

namespace mt::model::morph {

	std::vector<Mesh> &MorphModel::lod(int level)
	{
		if (level <= 0)
		{
			return meshes;
		}

		if (level > int(lods.size()))
		{
			throw mt_ex("Invalid level of detail: " + std::to_string(level));
		}

		return lods[level - 1];
	}

	int MorphModel::frameIndex() const
	{
		if (frames.empty())
		{
			return 0;
		}

		return std::clamp(currentFrame, 0, int(frames.size()) - 1);
	}

	static void validateMeshes(const std::vector<Mesh> &meshes, size_t numFrames)
	{
		for (const auto &mesh : meshes)
		{
			if (mesh.frames.size() != numFrames)
			{
				throw mt_ex("Mesh " + mesh.name + " has " + std::to_string(mesh.frames.size()) + " frames, expected "
							+ std::to_string(numFrames));
			}

			for (const auto &frame : mesh.frames)
			{
				if (frame.size() != mesh.numVertices())
				{
					throw mt_ex("Mesh " + mesh.name + " has a frame with the wrong number of vertices");
				}
			}

			const int numVertices = int(mesh.numVertices());
			for (const auto &tri : mesh.tris)
			{
				for (int i = 0; i < 3; i++)
				{
					if (tri[i] < 0 || tri[i] >= numVertices)
					{
						throw mt_ex("Mesh " + mesh.name + " has a triangle pointing to an invalid vertex: " + std::to_string(tri[i]));
					}
				}
			}

			if (!mesh.collapseMap.empty() && mesh.collapseMap.size() != mesh.numVertices())
			{
				throw mt_ex("Mesh " + mesh.name + " has an invalid collapse map");
			}
		}
	}

	void MorphModel::validate() const
	{
		validateMeshes(meshes, frames.size());

		for (const auto &level : lods)
		{
			validateMeshes(level, frames.size());
		}

		for (const auto &tag : tags)
		{
			if (tag.frames.size() != frames.size())
			{
				throw mt_ex("Tag " + tag.name + " does not have data for every frame");
			}
		}
	}
}
