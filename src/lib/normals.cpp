#include "normals.h"

namespace mt::model {
	using morph::FrameVertex;
	using morph::MorphModel;
	using skeletal::Joint;
	using skeletal::SkeletalModel;
	using skeletal::SkeletalVertex;
	using skeletal::Weight;

	static glm::vec3 safeNormalize(const glm::vec3 &vec)
	{
		float length = glm::length(vec);
		return length > 0.0f ? vec / length : glm::vec3{0, 0, 0};
	}

	// idTech models wind their triangles clockwise, the normal of (v1, v2, v3) is (v3 - v1) x (v2 - v1). This matches the
	// normals stored in MD3, MDC and MDM files and the old LegacyModelView.
	static glm::vec3 triangleNormal(const glm::vec3 &v1, const glm::vec3 &v2, const glm::vec3 &v3)
	{
		return safeNormalize(glm::cross(v3 - v1, v2 - v1));
	}

	/**
	 * Calculate normals from triangle data
	 * @param positions vertex positions
	 * @param tris triangles
	 * @param mode normal calculation mode
	 * @return normal per vertex
	 */
	static std::vector<glm::vec3> calculate(const std::vector<glm::vec3> &positions, const std::vector<glm::ivec3> &tris, NormalMode mode)
	{
		std::vector<glm::vec3> normals(positions.size(), glm::vec3{0, 0, 0});
		const int numVertices = int(positions.size());

		for (const auto &tri : tris)
		{
			for (int i = 0; i < 3; i++)
			{
				if (tri[i] < 0 || tri[i] >= numVertices)
				{
					throw mt_ex("Triangle points to an invalid vertex: " + std::to_string(tri[i]));
				}
			}

			glm::vec3 normal = triangleNormal(positions[tri[0]], positions[tri[1]], positions[tri[2]]);

			for (int i = 0; i < 3; i++)
			{
				if (mode == NormalMode::Smooth)
				{
					normals[tri[i]] += normal;
				}
				else
				{
					normals[tri[i]] = normal;
				}
			}
		}

		for (auto &normal : normals)
		{
			normal = safeNormalize(normal);
		}

		return normals;
	}

	static const Weight &vertexWeight(const skeletal::Mesh &mesh, const SkeletalVertex &vert, int index)
	{
		size_t offset = size_t(vert.startWeight) + size_t(index);
		if (vert.startWeight < 0 || offset >= mesh.weights.size())
		{
			throw mt_ex("Vertex points to an invalid weight: " + std::to_string(offset));
		}
		return mesh.weights[offset];
	}

	static const Point &jointPose(const SkeletalModel &model, int jointId, int frame)
	{
		if (jointId < 0 || jointId >= int(model.joints.size()))
		{
			throw mt_ex("Weight points to an invalid joint: " + std::to_string(jointId));
		}

		const Joint &joint = model.joints[jointId];
		if (frame == FRAME_BIND_POSE)
		{
			return joint;
		}

		if (frame < 0 || frame >= int(joint.frames.size()))
		{
			throw mt_ex("Invalid frame: " + std::to_string(frame));
		}

		return joint.frames[frame];
	}

	void calculateNormals(const SkeletalModel &model, skeletal::Mesh &mesh, int frame, NormalMode mode)
	{
		// Vertex positions in the requested pose
		std::vector<glm::vec3> positions(mesh.vertices.size());
		for (size_t v = 0; v < mesh.vertices.size(); v++)
		{
			const SkeletalVertex &vert = mesh.vertices[v];

			if (vert.weightCount <= 0)
			{
				positions[v] = vert.pos;
				continue;
			}

			glm::vec3 pos{0, 0, 0};
			for (int i = 0; i < vert.weightCount; i++)
			{
				const Weight &weight = vertexWeight(mesh, vert, i);
				const Point &joint = jointPose(model, weight.jointId, frame);
				pos += (joint.location + (joint.rotation * weight.pos)) * weight.bias;
			}
			positions[v] = pos;
		}

		setNormals(model, mesh, calculate(positions, mesh.tris, mode), frame);
	}

	void setNormals(const SkeletalModel &model, skeletal::Mesh &mesh, const std::vector<glm::vec3> &normals, int frame)
	{
		if (normals.size() != mesh.vertices.size())
		{
			throw mt_ex("Normal count does not match the vertex count");
		}

		for (size_t v = 0; v < mesh.vertices.size(); v++)
		{
			SkeletalVertex &vert = mesh.vertices[v];

			if (frame == FRAME_BIND_POSE)
			{
				vert.bindNormal = normals[v];
			}

			if (vert.weightCount <= 0)
			{
				vert.normal = normals[v];
				continue;
			}

			// The animated normal is blend * vert.normal, where blend is the weighted sum of the joint rotations.
			// Store the normal through the inverse of the blend, so the pose used here gives back the exact normal.
			glm::mat3 blend{0.0f};
			glm::vec3 approximation{0, 0, 0};
			for (int i = 0; i < vert.weightCount; i++)
			{
				const Weight &weight = vertexWeight(mesh, vert, i);
				const Point &joint = jointPose(model, weight.jointId, frame);
				blend += glm::mat3_cast(joint.rotation) * weight.bias;
				approximation += (glm::inverse(joint.rotation) * normals[v]) * weight.bias;
			}

			if (std::abs(glm::determinant(blend)) > 1e-6f)
			{
				vert.normal = safeNormalize(glm::inverse(blend) * normals[v]);
			}
			else
			{
				// The joint rotations cancel each other out, fall back to the weighted joint-local normal
				vert.normal = safeNormalize(approximation);
			}
		}
	}

	bool calculateNormals(SkeletalModel &model, NormalMode mode)
	{
		int frame = FRAME_BIND_POSE;

		if (!model.hasBindPose)
		{
			if (!model.hasFrames())
			{
				return false;
			}

			frame = std::clamp(model.currentFrame, 0, model.numFrames() - 1);
		}

		for (auto &mesh : model.meshes)
		{
			calculateNormals(model, mesh, frame, mode);
		}

		return true;
	}

	static void calculateMorphMeshes(std::vector<morph::Mesh> &meshes, NormalMode mode)
	{
		for (auto &mesh : meshes)
		{
			for (auto &frame : mesh.frames)
			{
				std::vector<glm::vec3> positions(frame.size());
				std::transform(frame.begin(), frame.end(), positions.begin(), [](const FrameVertex &vert) { return vert.pos; });

				std::vector<glm::vec3> normals = calculate(positions, mesh.tris, mode);

				for (size_t v = 0; v < frame.size(); v++)
				{
					frame[v].normal = normals[v];
				}
			}
		}
	}

	void calculateNormals(MorphModel &model, NormalMode mode)
	{
		calculateMorphMeshes(model.meshes, mode);

		for (auto &level : model.lods)
		{
			calculateMorphMeshes(level, mode);
		}
	}

	bool calculateNormals(Model &model, NormalMode mode)
	{
		if (auto *skeletal = dynamic_cast<SkeletalModel *>(&model))
		{
			return calculateNormals(*skeletal, mode);
		}

		if (auto *morph = dynamic_cast<MorphModel *>(&model))
		{
			calculateNormals(*morph, mode);
			return true;
		}

		return false;
	}
}
