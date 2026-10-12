#include "model_ms3d.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "et_armature.h"
#include "binary_reader.h"
#include "normals.h"

#include <filesystem>
#include <map>
#include <tuple>

namespace mt::model {
	using namespace mt::model::skeletal;
	using mt::io::BinaryReader;

	namespace {
		struct Ms3dVertex {
			glm::vec3 pos{};
			int8_t boneIds[4]{-1, -1, -1, -1};
			uint8_t weights[3]{};
		};

		struct Ms3dTriangle {
			uint16_t vertexIndices[3]{};
			glm::vec3 normals[3]{};
			float s[3]{};
			float t[3]{};
		};

		struct Ms3dGroup {
			std::string name;
			std::vector<uint16_t> triangles;
			int8_t materialIndex = -1;
		};

		struct Ms3dKeyframe {
			float frame{}; ///< keyframe time converted to frames
			glm::vec3 value{};
		};

		struct Ms3dJoint {
			std::string name;
			std::string parentName;
			glm::vec3 rotation{}; ///< euler angles in radians
			glm::vec3 position{};
			std::vector<Ms3dKeyframe> rotationKeys;
			std::vector<Ms3dKeyframe> positionKeys;
			int parent = -1;
		};

		struct Ms3dFile {
			std::vector<Ms3dVertex> vertices;
			std::vector<Ms3dTriangle> triangles;
			std::vector<Ms3dGroup> groups;
			std::vector<std::string> materialTextures;
			float fps = 1.0f;
			int totalFrames = 0;
			std::vector<Ms3dJoint> joints;
			int weightSubVersion = 0; ///< 0 = no extra weights, 1 = weights 0-255, 2+ = weights 0-100
		};

		glm::vec3 readVec3(BinaryReader &reader)
		{
			auto values = reader.readArray<float>(3);
			return {values[0], values[1], values[2]};
		}

		void skipComments(BinaryReader &reader, bool indexed)
		{
			auto count = reader.read<int32_t>();
			if (count < 0)
			{
				throw mt_ex("Invalid MS3D comment count");
			}

			for (int32_t i = 0; i < count; i++)
			{
				if (indexed)
				{
					reader.read<int32_t>();
				}
				reader.skip(reader.read<int32_t>());
			}
		}

		Ms3dFile parse(const Ref<mt::io::MTFile> &file)
		{
			BinaryReader reader(file);
			Ms3dFile out;

			if (reader.readString(10) != "MS3D000000")
			{
				throw mt_ex("Not an MS3D file");
			}

			auto version = reader.read<int32_t>();
			if (version != 4)
			{
				throw mt_ex("Unsupported MS3D version: " + std::to_string(version));
			}

			out.vertices.resize(reader.read<uint16_t>());
			for (auto &vert : out.vertices)
			{
				reader.read<uint8_t>(); // flags
				vert.pos = readVec3(reader);
				vert.boneIds[0] = reader.read<int8_t>();
				reader.read<uint8_t>(); // reference count
			}

			out.triangles.resize(reader.read<uint16_t>());
			for (auto &tri : out.triangles)
			{
				reader.read<uint16_t>(); // flags
				for (auto &index : tri.vertexIndices)
				{
					index = reader.read<uint16_t>();
					if (index >= out.vertices.size())
					{
						throw mt_ex("Triangle points to an invalid vertex: " + std::to_string(index));
					}
				}
				for (auto &normal : tri.normals)
				{
					normal = readVec3(reader);
				}
				for (auto &s : tri.s)
				{
					s = reader.read<float>();
				}
				for (auto &t : tri.t)
				{
					t = reader.read<float>();
				}
				reader.read<uint8_t>(); // smoothing group
				reader.read<uint8_t>(); // group index
			}

			out.groups.resize(reader.read<uint16_t>());
			for (auto &group : out.groups)
			{
				reader.read<uint8_t>(); // flags
				group.name = reader.readString(32);
				group.triangles = reader.readArray<uint16_t>(reader.read<uint16_t>());
				group.materialIndex = reader.read<int8_t>();

				for (auto index : group.triangles)
				{
					if (index >= out.triangles.size())
					{
						throw mt_ex("Group " + group.name + " points to an invalid triangle: " + std::to_string(index));
					}
				}
			}

			auto numMaterials = reader.read<uint16_t>();
			for (uint16_t i = 0; i < numMaterials; i++)
			{
				reader.skip(32 + 16 * 4 + 4 + 4 + 1); // name, colors, shininess, transparency, mode
				out.materialTextures.push_back(reader.readString(128));
				reader.skip(128); // alpha map
			}

			out.fps = std::max(reader.read<float>(), 1.0f);
			reader.read<float>(); // current time
			out.totalFrames = reader.read<int32_t>();
			if (out.totalFrames < 0)
			{
				throw mt_ex("Invalid MS3D frame count: " + std::to_string(out.totalFrames));
			}

			out.joints.resize(reader.read<uint16_t>());
			for (auto &joint : out.joints)
			{
				reader.read<uint8_t>(); // flags
				joint.name = reader.readString(32);
				joint.parentName = reader.readString(32);
				joint.rotation = readVec3(reader);
				joint.position = readVec3(reader);

				auto numRotationKeys = reader.read<uint16_t>();
				auto numPositionKeys = reader.read<uint16_t>();

				auto readKeys = [&](uint16_t count, std::vector<Ms3dKeyframe> &keys) {
					for (uint16_t k = 0; k < count; k++)
					{
						Ms3dKeyframe key;
						key.frame = reader.read<float>() * out.fps;
						key.value = readVec3(reader);
						keys.push_back(key);
					}
				};
				readKeys(numRotationKeys, joint.rotationKeys);
				readKeys(numPositionKeys, joint.positionKeys);
			}

			// Optional extra sections, each starts with a sub version
			if (reader.remaining() > 0)
			{
				if (reader.read<int32_t>() != 1)
				{
					throw mt_ex("Unsupported MS3D comment sub version");
				}
				skipComments(reader, true);	 // groups
				skipComments(reader, true);	 // materials
				skipComments(reader, true);	 // joints
				skipComments(reader, false); // model
			}

			if (reader.remaining() > 0)
			{
				out.weightSubVersion = reader.read<int32_t>();
				if (out.weightSubVersion < 1 || out.weightSubVersion > 3)
				{
					throw mt_ex("Unsupported MS3D vertex extra sub version: " + std::to_string(out.weightSubVersion));
				}

				for (auto &vert : out.vertices)
				{
					for (int i = 1; i < 4; i++)
					{
						vert.boneIds[i] = reader.read<int8_t>();
					}
					for (auto &weight : vert.weights)
					{
						weight = reader.read<uint8_t>();
					}
					reader.skip(out.weightSubVersion >= 2 ? 4 : 0); // extra
					reader.skip(out.weightSubVersion >= 3 ? 4 : 0); // extra2
				}
			}

			// Joint colors and model extras are not used

			return out;
		}

		glm::quat eulerToQuat(const glm::vec3 &angles)
		{
			// MilkShape rotates around X, then Y, then Z (radians)
			return glm::normalize(glm::angleAxis(angles.z, glm::vec3{0, 0, 1}) * glm::angleAxis(angles.y, glm::vec3{0, 1, 0})
								  * glm::angleAxis(angles.x, glm::vec3{1, 0, 0}));
		}

		/**
		 * Value of an animation channel at a frame, interpolated between the keys
		 * @param keys keys sorted by time
		 * @param frame frame number (key time * fps)
		 * @param fallback value when there are no keys
		 * @param convert converts the key value
		 * @param interpolate interpolates between two converted values
		 */
		template<typename T>
		T sampleKeys(const std::vector<Ms3dKeyframe> &keys, float frame, T fallback, T (*convert)(const glm::vec3 &),
					 T (*interpolate)(const T &, const T &, float))
		{
			if (keys.empty())
			{
				return fallback;
			}

			if (frame <= keys.front().frame)
			{
				return convert(keys.front().value);
			}

			if (frame >= keys.back().frame)
			{
				return convert(keys.back().value);
			}

			for (size_t i = 0; i + 1 < keys.size(); i++)
			{
				const auto &a = keys[i];
				const auto &b = keys[i + 1];
				if (frame >= a.frame && frame <= b.frame)
				{
					float length = b.frame - a.frame;
					float t = length > 0.0f ? (frame - a.frame) / length : 0.0f;
					return interpolate(convert(a.value), convert(b.value), t);
				}
			}

			return convert(keys.back().value);
		}

		glm::vec3 identityVec(const glm::vec3 &value)
		{
			return value;
		}

		glm::vec3 lerpVec(const glm::vec3 &a, const glm::vec3 &b, float t)
		{
			return glm::mix(a, b, t);
		}

		glm::quat slerpQuat(const glm::quat &a, const glm::quat &b, float t)
		{
			return glm::normalize(glm::slerp(a, b, t));
		}

		void resolveParents(Ms3dFile &file)
		{
			for (size_t i = 0; i < file.joints.size(); i++)
			{
				auto &joint = file.joints[i];
				if (joint.parentName.empty())
				{
					continue;
				}

				auto parent
					= std::find_if(file.joints.begin(), file.joints.end(), [&](const Ms3dJoint &j) { return j.name == joint.parentName; });
				if (parent == file.joints.end())
				{
					throw mt_ex("Parent joint not found for " + joint.name + ": " + joint.parentName);
				}

				joint.parent = int(parent - file.joints.begin());
				if (joint.parent >= int(i))
				{
					throw mt_ex("Joint " + joint.name + " comes before its parent " + joint.parentName);
				}
			}
		}

		/**
		 * Build the animation frames, the keys are relative to the joint's rest pose
		 * @param file parsed file
		 * @param joints joints to add the frames to (same order as the file)
		 */
		void buildFrames(const Ms3dFile &file, std::vector<Joint> &joints)
		{
			for (int frame = 0; frame < file.totalFrames; frame++)
			{
				// Key times are seconds, frame f is at f / fps (the sample files have keys from frame 0)
				const float time = float(frame);

				for (size_t j = 0; j < file.joints.size(); j++)
				{
					const auto &source = file.joints[j];

					glm::vec3 keyPos = sampleKeys<glm::vec3>(source.positionKeys, time, glm::vec3{0, 0, 0}, identityVec, lerpVec);
					glm::quat keyRot = sampleKeys<glm::quat>(source.rotationKeys, time, glm::quat{1, 0, 0, 0}, eulerToQuat, slerpQuat);

					glm::quat restRot = eulerToQuat(source.rotation);
					Point point;
					point.location = source.position + restRot * keyPos;
					point.rotation = glm::normalize(restRot * keyRot);

					if (source.parent >= 0)
					{
						const Point &parent = joints[source.parent].frames.back();
						point.location = parent.location + parent.rotation * point.location;
						point.rotation = glm::normalize(parent.rotation * point.rotation);
					}

					joints[j].frames.push_back(point);
				}
			}
		}

		std::string fileStem(const Ref<mt::io::MTFile> &file)
		{
			return std::filesystem::path(file->name).stem().string();
		}
	}

	Model *loadMS3D(const Ref<mt::io::MTFile> &file)
	{
		Ms3dFile source = parse(file);
		resolveParents(source);

		auto model = std::make_unique<SkeletalModel>();
		model->name = fileStem(file);
		model->hasBindPose = true;
		model->flipBoneTags = true;

		for (const auto &src : source.joints)
		{
			Joint joint;
			joint.name = src.name;
			joint.parentId = src.parent;
			joint.location = src.position;
			joint.rotation = eulerToQuat(src.rotation);

			if (joint.parentId >= 0)
			{
				const Joint &parent = model->joints[joint.parentId];
				joint.location = parent.location + parent.rotation * joint.location;
				joint.rotation = glm::normalize(parent.rotation * joint.rotation);
			}

			model->joints.push_back(joint);
		}

		const float weightScale = source.weightSubVersion == 1 ? 255.0f : 100.0f;

		for (const auto &group : source.groups)
		{
			Mesh mesh;
			mesh.name = group.name;
			if (group.materialIndex >= 0 && size_t(group.materialIndex) < source.materialTextures.size())
			{
				mesh.materialName = source.materialTextures[group.materialIndex];
			}

			// Texture coordinates and normals are stored per triangle corner, so a file vertex can become several vertices
			std::map<std::tuple<uint16_t, float, float, float, float, float>, int> corners;
			std::vector<glm::vec3> normals;

			for (auto triIndex : group.triangles)
			{
				const auto &tri = source.triangles[triIndex];
				glm::ivec3 out;

				for (int c = 0; c < 3; c++)
				{
					const glm::vec3 normal = tri.normals[c];
					auto key = std::make_tuple(tri.vertexIndices[c], tri.s[c], tri.t[c], normal.x, normal.y, normal.z);
					auto found = corners.find(key);
					if (found != corners.end())
					{
						out[c] = found->second;
						continue;
					}

					const auto &vert = source.vertices[tri.vertexIndices[c]];
					SkeletalVertex vertex;
					vertex.pos = vert.pos;
					vertex.tex = {tri.s[c], tri.t[c]};
					vertex.startWeight = int(mesh.weights.size());

					if (!model->joints.empty())
					{
						// Up to 4 bones, the last weight is what is left from the others
						int weights[4] = {vert.weights[0], vert.weights[1], vert.weights[2], 0};
						if (source.weightSubVersion == 0 || (weights[0] == 0 && weights[1] == 0 && weights[2] == 0))
						{
							weights[0] = int(weightScale);
						}
						weights[3] = int(weightScale) - weights[0] - weights[1] - weights[2];

						for (int b = 0; b < 4; b++)
						{
							int boneId = vert.boneIds[b];
							if (weights[b] <= 0 || boneId < 0)
							{
								continue;
							}

							if (boneId >= int(model->joints.size()))
							{
								throw mt_ex("Vertex " + std::to_string(tri.vertexIndices[c])
											+ " has an invalid bone: " + std::to_string(boneId));
							}

							const Joint &joint = model->joints[boneId];
							glm::vec3 local = glm::inverse(joint.rotation) * (vertex.pos - joint.location);
							mesh.weights.push_back({boneId, float(weights[b]) / weightScale, local});
							vertex.weightCount++;
						}

						if (vertex.weightCount == 0)
						{
							throw mt_ex("Vertex " + std::to_string(tri.vertexIndices[c]) + " is not bound to a bone");
						}
					}

					out[c] = int(mesh.vertices.size());
					corners.emplace(key, out[c]);
					mesh.vertices.push_back(vertex);
					normals.push_back(glm::length(normal) > 0.0f ? glm::normalize(normal) : normal);
				}

				mesh.tris.push_back(out);
			}

			// Some exporters leave the normals empty
			bool hasNormals = std::any_of(normals.begin(), normals.end(), [](const glm::vec3 &n) { return glm::length(n) > 0.0f; });
			if (hasNormals)
			{
				setNormals(*model, mesh, normals, FRAME_BIND_POSE);
			}
			else
			{
				calculateNormals(*model, mesh, FRAME_BIND_POSE);
			}
			mesh.minLod = int(mesh.vertices.size());
			model->meshes.push_back(std::move(mesh));
		}

		buildFrames(source, model->joints);

		optimizeVertices(*model);
		for (auto &mesh : model->meshes)
		{
			mesh.minLod = int(mesh.vertices.size());
		}

		et::applyDefaultJointInformation(*model);
		et::markTagBones(*model);

		// MilkShape is Y-up, the game is Z-up
		model->transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{1, 0, 0});

		if (source.totalFrames > 0)
		{
			calculateBounds(*model, 1.2f);

			Animation animation;
			animation.name = model->name;
			animation.startFrame = 0;
			animation.numFrames = source.totalFrames;
			animation.fps = int(std::lround(source.fps));
			model->animations.push_back(animation);
		}

		return model.release();
	}

	void loadMS3DAnimation(Model *parent, const Ref<mt::io::MTFile> &file)
	{
		auto *model = dynamic_cast<SkeletalModel *>(parent);
		if (!model)
		{
			throw mt_ex("MS3D animations can only be loaded into a skeletal model");
		}

		Ms3dFile source = parse(file);
		resolveParents(source);

		if (source.joints.size() != model->joints.size())
		{
			throw mt_ex("The animation has " + std::to_string(source.joints.size()) + " joints, the model has "
						+ std::to_string(model->joints.size()));
		}

		for (size_t i = 0; i < source.joints.size(); i++)
		{
			if (source.joints[i].name != model->joints[i].name || source.joints[i].parent != model->joints[i].parentId)
			{
				throw mt_ex("Joints do not match: " + model->joints[i].name + " != " + source.joints[i].name);
			}
		}

		const int startFrame = model->numFrames();

		// Build into a copy so a broken file does not leave the model half updated
		std::vector<Joint> joints = model->joints;
		buildFrames(source, joints);
		model->joints = std::move(joints);

		if (source.totalFrames > 0)
		{
			calculateBounds(*model, 1.2f, startFrame);

			Animation animation;
			animation.name = fileStem(file);
			animation.startFrame = startFrame;
			animation.numFrames = source.totalFrames;
			animation.fps = int(std::lround(source.fps));
			model->animations.push_back(animation);
		}
	}
}
