#include "ModelMD5.h"
#include "SkeletalModel.h"
#include "ScriptStream.h"
#include "glm/gtx/normal.hpp"

namespace mt::model {
	using namespace Skeletal;

	// There should not be any other versions
	const int MD5_VERSION = 10;

	static void computeQuaternionWComponent(glm::quat &quat)
	{
		float comp = 1.0f - (quat.x * quat.x) - (quat.y * quat.y) - (quat.z * quat.z);
		if (comp < 0.0f)
		{
			quat.w = 0.0f;
		}
		else
		{
			quat.w = -std::sqrt(comp);
		}
	}

	static void parseMeshPart(ScriptStream &stream, SkeletalModel *model)
	{
		if (stream.token() != "{")
		{
			throw std::invalid_argument("Invalid token: " + stream.last());
		}

		Mesh mesh;

		int numVerts = 0;
		int numTris = 0;
		int numWeights = 0;

		while (stream.token() != "}")
		{
			switch (hash(stream.last().c_str()))
			{
				case hash("shader"):
					// A mesh does not have a name per say, but usually the material name is the same as the mesh name
					mesh.name = mesh.material.name = stream.token();
					break;

				case hash("numverts"):
					numVerts = stream.parseInt();
					break;

				case hash("vert"):
				{
					SkeletalVertex vertex;
					int offset = stream.parseInt();
					stream.parse1DMatrix(2, glm::value_ptr(vertex.tex));
					vertex.startWeight = stream.parseInt();
					vertex.weightCount = stream.parseInt();

					if (vertex.startWeight < 0 || vertex.weightCount <= 0)
					{
						throw std::invalid_argument("Invalid vertex");
					}

					mesh.vertices.emplace_back(vertex);
					break;
				}

				case hash("numtris"):
					numTris = stream.parseInt();
					break;

				case hash("tri"):
				{
					glm::ivec3 tri;

					int triangleOffset = stream.parseInt();

					if (triangleOffset != mesh.tris.size())
					{
						throw std::invalid_argument("Invalid triangle offset: " + std::to_string(triangleOffset));
					}

					tri.x = stream.parseInt();
					tri.y = stream.parseInt();
					tri.z = stream.parseInt();

					mesh.tris.emplace_back(tri);
					break;
				}

				case hash("numweights"):
					numWeights = stream.parseInt();
					break;

				case hash("weight"):
				{
					Weight weight;


					if (stream.parseInt() < 0)
					{
						throw std::invalid_argument("Invalid weight offset");
					}

					weight.jointId = stream.parseInt();
					weight.bias = stream.parseFloat();
					stream.parse1DMatrix(3, glm::value_ptr(weight.pos));

					mesh.weights.emplace_back(weight);
					break;
				}

				default:
				{
					throw std::invalid_argument("Unknown token: " + stream.last());
				}
			}
		}

		if (numVerts < 0 || numVerts != mesh.vertices.size())
		{
			throw std::invalid_argument("Invalid amount of vertices: " + std::to_string(numVerts));
		}

		if (numTris < 0 || numTris != mesh.tris.size())
		{
			throw std::invalid_argument("Invalid amount of triangles: " + std::to_string(numTris));
		}

		if (numWeights < 0 || numWeights != mesh.weights.size())
		{
			throw std::invalid_argument("Invalid amount of weights: " + std::to_string(numWeights));
		}

		// Compute vertex positions
		for (auto &vert : mesh.vertices)
		{
			// Make sure the position and the normal values are zeroed
			vert.pos = {0, 0, 0};
			vert.normal = {0, 0, 0};

			// Sum the position of the weights
			for (int i = 0; i < vert.weightCount; i++)
			{
				auto &weight = mesh.weights[vert.startWeight + i];
				auto &joint = model->joints[weight.jointId];

				// Convert the weight positions from joint local space to object space
				glm::vec3 rotatedPosition = joint.rotation * weight.pos;
				vert.pos += (joint.location + rotatedPosition) * weight.bias;
			}
		}

		// Calculate triangle normals
		for (auto &tri : mesh.tris)
		{
			auto &v1 = mesh.vertices[tri[0]];
			auto &v2 = mesh.vertices[tri[1]];
			auto &v3 = mesh.vertices[tri[2]];

			glm::vec3 normal = glm::triangleNormal(v1.pos, v2.pos, v3.pos);

			v1.normal += normal;
			v2.normal += normal;
			v3.normal += normal;
		}

		// normalize
		for (auto &vert : mesh.vertices)
		{
			vert.normal = glm::normalize(vert.normal);
			glm::vec3 normal = vert.normal;
			vert.bindNormal = vert.normal;

			// Reset the current normal
			vert.normal = {0, 0, 0};

			// Put the bind-pose normal into joint-local space
			// so the animated normal can be computed faster later
			for (int i = 0; i < vert.weightCount; i++)
			{
				auto &weight = mesh.weights[vert.startWeight + i];
				auto &joint = model->joints[weight.jointId];

				glm::vec3 tmp = normal * joint.rotation;
				tmp = tmp + (tmp * weight.bias);
				vert.normal += tmp;
			}

			vert.normal = glm::normalize(vert.normal);
		}

		model->meshes.emplace_back(mesh);
	}

	static void parseMeshRoot(ScriptStream &stream, SkeletalModel *model)
	{
		int numJoints = -1;
		int numMeshes = -1;

		// MD5 models do have a bind pose by default
		model->hasBindPose = true;

		while (stream.hasNext())
		{
			std::string token = stream.token();

			switch (hash(token.c_str()))
			{
				// Check that the version matches the one version that we actually support (this should always be the same)
				case hash("MD5Version"):
					if (stream.parseInt() != MD5_VERSION)
					{
						throw std::invalid_argument("Invalid MD5 version: " + stream.last());
					}
					break;

					// This can be ignored
				case hash("commandline"):
					// Skip the rest of the line, we do not care about the command line used
					stream.skipLine();
					break;

				case hash("numJoints"):
					numJoints = (int) stream.parse();
					break;

				case hash("numMeshes"):
					numMeshes = (int) stream.parse();
					break;

				case hash("joints"):
				{
					if (stream.token() != "{")
					{
						throw std::invalid_argument("Invalid joint data");
					}

					while (stream.token() != "}")
					{
						Joint joint;
						joint.name = stream.last();
						joint.parentId = stream.parseInt();
						stream.parse1DMatrix(3, glm::value_ptr(joint.location));
						stream.parse1DMatrix(3, glm::value_ptr(joint.rotation));
						computeQuaternionWComponent(joint.rotation);
						model->joints.push_back(joint);
					}
					break;
				}
				case hash("mesh"):
				{
					parseMeshPart(stream, model);
					break;
				}

				default:
				{
					throw std::invalid_argument("Unknown token: " + token);
				}
			}
		}

		if (model->meshes.size() != numMeshes)
		{
			throw std::invalid_argument("invalid number of meshes");
		}

		if (model->joints.size() != numJoints)
		{
			throw std::invalid_argument("invalid number of joints");
		}
	}

	Model *loadMd5Model(const Ref<IO::MTFile> &file)
	{
		// MD5 model file is just a text file
		ScriptStream stream(file);
		auto *model = new SkeletalModel;

		try
		{
			parseMeshRoot(stream, model);
		} catch (const std::exception &exception)
		{
			delete model;
			return nullptr;
		}

		return model;
	}

	static void parseAnimRoot(ScriptStream &stream, SkeletalModel *model)
	{
		int numFrames = -1;
		int frameRate = -1;
		int numAnimatedComponents = -1;

		int processedFrames = 0;

		while (stream.hasNext())
		{
			std::string token = stream.token();

			switch (hash(token.c_str()))
			{
				// Check that the version matches the one version that we actually support (this should always be the same)
				case hash("MD5Version"):
					if ((int) stream.parse() != MD5_VERSION)
					{
						throw std::invalid_argument("Invalid MD5 version: " + stream.last());
					}
					break;

					// This can be ignored
				case hash("commandline"):
					// Skip the rest of the line, we do not care about the command line used
					stream.skipLine();
					break;

				case hash("numFrames"):
					numFrames = stream.parseInt();
					break;

				case hash("numJoints"):
					if (model->joints.size() != stream.parseInt())
					{
						throw std::invalid_argument("Invalid number of joints expected: " + stream.last());
					}
					break;

				case hash("frameRate"):
					frameRate = stream.parseInt();
					break;

				case hash("numAnimatedComponents"):
					numAnimatedComponents = stream.parseInt();
					break;

				case hash("hierarchy"):
				{
					if (stream.token() != "{")
					{
						throw std::invalid_argument("Invalid hierarchy data");
					}

					for (auto &joint : model->joints)
					{
						if (joint.name != stream.token())
						{
							throw std::invalid_argument(
									"Invalid joint hierarchy, name does not match: " + joint.name + " != " +
									stream.last());
						}

						if (joint.parentId != stream.parseInt())
						{
							throw std::invalid_argument(
									"Invalid joint hierarchy, parent ID does not match for: " + joint.name);
						}

						joint.flags = stream.parseInt();
						joint.startIndex = stream.parseInt();
					}

					if (stream.token() != "}")
					{
						throw std::invalid_argument("Invalid number of joints");
					}

					break;
				}

				case hash("bounds"):
				{
					if (stream.token() != "{")
					{
						throw std::invalid_argument("Invalid bounds data");
					}

					while (stream.peekNext() != "}")
					{
						Bounds bound{};
						stream.parse1DMatrix(3, glm::value_ptr(bound.min));
						stream.parse1DMatrix(3, glm::value_ptr(bound.max));
						model->bounds.emplace_back(bound);
					}
					stream.parse();
					break;
				}

				case hash("baseframe"):
					// The model should already have a bind pose, so just skip this section
					stream.skipSection();
					break;

				case hash("frame"):
				{
					int frameNumber = stream.parseInt();
					std::vector<float> frameValues{};

					if (stream.token() != "{")
					{
						throw std::invalid_argument("Invalid frame data for frame: " + std::to_string(frameNumber));
					}

					while (stream.peekNext() != "}")
					{
						frameValues.emplace_back(stream.parseFloat());
					}
					stream.parse();

					// calculate the final joint locations per frame
					for (auto &joint : model->joints)
					{
						Point point;

						unsigned int j = 0;

						if (joint.flags & 1) // Pos.x
						{
							point.location.x = frameValues[joint.startIndex + j++];
						}
						if (joint.flags & 2) // Pos.y
						{
							point.location.y = frameValues[joint.startIndex + j++];
						}
						if (joint.flags & 4) // Pos.z
						{
							point.location.z = frameValues[joint.startIndex + j++];
						}
						if (joint.flags & 8) // Orient.x
						{
							point.rotation.x = frameValues[joint.startIndex + j++];
						}
						if (joint.flags & 16) // Orient.y
						{
							point.rotation.y = frameValues[joint.startIndex + j++];
						}
						if (joint.flags & 32) // Orient.z
						{
							point.rotation.z = frameValues[joint.startIndex + j++];
						}

						computeQuaternionWComponent(point.rotation);

						// Has a parent joint so we need to calculate the position based on the parent position
						if (joint.parentId >= 0)
						{
							Point &parentJoint = model->joints[joint.parentId].frames.back();

							point.location = parentJoint.location + (parentJoint.rotation * point.location);
							point.rotation = glm::normalize(parentJoint.rotation * point.rotation);
						}

						joint.frames.push_back(point);
					}

					processedFrames++;

					break;
				}

				default:
				{
					throw std::invalid_argument("Unknown token: " + token);
				}
			}
		}

		if (numFrames != processedFrames)
		{
			throw std::invalid_argument(
					"Invalid animation data. Framecounts do not match: " + std::to_string(numFrames) + " != " +
					std::to_string(processedFrames));
		}
	}

	void loadMD5Anim(Model *baseModel, const Ref<IO::MTFile> &file)
	{
		auto *model = dynamic_cast<SkeletalModel *>(baseModel);

		// MD5 model file is just a text file
		ScriptStream stream(file);

		try
		{
			parseAnimRoot(stream, model);
		} catch (const std::exception &ex)
		{
			// FIXME: handle
		}
	}
}
