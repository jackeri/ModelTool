#include "model_iqm.h"
#include "skeletal_model.h"
#include "binary_reader.h"
#include "normals.h"

#include <cstring>
#include <filesystem>

namespace mt::model {
	using namespace mt::model::skeletal;
	using mt::io::BinaryReader;

	namespace {
		const char IQM_MAGIC[16] = "INTERQUAKEMODEL";
		const uint32_t IQM_VERSION = 2;

		struct iqmHeader_t {
			char magic[16];
			uint32_t version;
			uint32_t filesize;
			uint32_t flags;
			uint32_t num_text, ofs_text;
			uint32_t num_meshes, ofs_meshes;
			uint32_t num_vertexarrays, num_vertexes, ofs_vertexarrays;
			uint32_t num_triangles, ofs_triangles, ofs_adjacency;
			uint32_t num_joints, ofs_joints;
			uint32_t num_poses, ofs_poses;
			uint32_t num_anims, ofs_anims;
			uint32_t num_frames, num_framechannels, ofs_frames, ofs_bounds;
			uint32_t num_comment, ofs_comment;
			uint32_t num_extensions, ofs_extensions;
		};

		struct iqmMesh_t {
			uint32_t name;
			uint32_t material;
			uint32_t first_vertex, num_vertexes;
			uint32_t first_triangle, num_triangles;
		};

		enum VertexArrayType : uint32_t
		{
			IQM_POSITION = 0,
			IQM_TEXCOORD = 1,
			IQM_NORMAL = 2,
			IQM_TANGENT = 3,
			IQM_BLENDINDEXES = 4,
			IQM_BLENDWEIGHTS = 5,
			IQM_COLOR = 6,
		};

		enum VertexArrayFormat : uint32_t
		{
			IQM_BYTE = 0,
			IQM_UBYTE = 1,
			IQM_SHORT = 2,
			IQM_USHORT = 3,
			IQM_INT = 4,
			IQM_UINT = 5,
			IQM_HALF = 6,
			IQM_FLOAT = 7,
			IQM_DOUBLE = 8,
		};

		struct iqmTriangle_t {
			uint32_t vertex[3];
		};

		struct iqmJoint_t {
			uint32_t name;
			int32_t parent;
			float translate[3], rotate[4], scale[3];
		};

		struct iqmPose_t {
			int32_t parent;
			uint32_t mask;
			float channeloffset[10];
			float channelscale[10];
		};

		struct iqmAnim_t {
			uint32_t name;
			uint32_t first_frame, num_frames;
			float framerate;
			uint32_t flags;
		};

		const uint32_t IQM_LOOP = 1u << 0u;

		struct iqmVertexArray_t {
			uint32_t type;
			uint32_t flags;
			uint32_t format;
			uint32_t size;
			uint32_t offset;
		};

		struct iqmBounds_t {
			float bbmin[3], bbmax[3];
			float xyradius, radius;
		};

		static_assert(sizeof(iqmHeader_t) == 124);
		static_assert(sizeof(iqmJoint_t) == 48);
		static_assert(sizeof(iqmPose_t) == 88);
		static_assert(sizeof(iqmMesh_t) == 24);
		static_assert(sizeof(iqmAnim_t) == 20);
		static_assert(sizeof(iqmBounds_t) == 32);

		/*! Strings are stored as offsets into a null separated text block */
		class TextBlock {
		  public:
			TextBlock(const BinaryReader &reader, const iqmHeader_t &header)
			{
				if (header.num_text > 0)
				{
					auto block = reader.sub(header.ofs_text, header.num_text);
					text = block.readArray<char>(header.num_text);
				}
			}

			[[nodiscard]] std::string get(uint32_t offset) const
			{
				if (offset >= text.size())
				{
					if (text.empty() && offset == 0)
					{
						return "";
					}
					throw mt_ex("Invalid text offset: " + std::to_string(offset));
				}

				return {text.data() + offset, strnlen(text.data() + offset, text.size() - offset)};
			}

		  private:
			std::vector<char> text;
		};

		/**
		 * Read a vertex array converted to floats
		 * @return components * num_vertexes values
		 */
		std::vector<float>
		readVertexArray(const BinaryReader &reader, const iqmVertexArray_t &array, uint32_t components, uint32_t numVertexes)
		{
			if (array.size != components)
			{
				throw mt_ex("Unsupported IQM vertex array size " + std::to_string(array.size) + " for type " + std::to_string(array.type));
			}

			const int64_t count = int64_t(components) * numVertexes;
			BinaryReader data = reader.from(array.offset);
			std::vector<float> out;

			switch (array.format)
			{
			case IQM_FLOAT: return data.readArray<float>(count);
			case IQM_UBYTE: {
				auto values = data.readArray<uint8_t>(count);
				out.assign(values.begin(), values.end());
				return out;
			}
			case IQM_INT: {
				auto values = data.readArray<int32_t>(count);
				out.assign(values.begin(), values.end());
				return out;
			}
			case IQM_UINT: {
				auto values = data.readArray<uint32_t>(count);
				out.assign(values.begin(), values.end());
				return out;
			}
			default:
				throw mt_ex("Unsupported IQM vertex array format " + std::to_string(array.format) + " for type "
							+ std::to_string(array.type));
			}
		}

		glm::quat toQuat(const float (&rotate)[4])
		{
			// IQM stores x y z w
			return glm::normalize(glm::quat(rotate[3], rotate[0], rotate[1], rotate[2]));
		}
	}

	Model *loadIQM(const Ref<mt::io::MTFile> &file)
	{
		BinaryReader reader(file);
		auto header = reader.read<iqmHeader_t>();

		if (std::memcmp(header.magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
		{
			throw mt_ex("Not an IQM file");
		}

		if (header.version != IQM_VERSION)
		{
			throw mt_ex("Unsupported IQM version: " + std::to_string(header.version));
		}

		if (header.filesize > reader.size())
		{
			throw mt_ex("The IQM file is truncated");
		}

		TextBlock text(reader, header);

		auto model = std::make_unique<SkeletalModel>();
		model->name = std::filesystem::path(file->name).stem().string();
		model->hasBindPose = true;

		// Joints, the bind pose is stored relative to the parent joint
		reader.seek(header.ofs_joints);
		auto joints = reader.readArray<iqmJoint_t>(header.num_joints);
		for (size_t i = 0; i < joints.size(); i++)
		{
			const auto &source = joints[i];
			if (source.parent >= int32_t(i))
			{
				throw mt_ex("Joint " + std::to_string(i) + " has an invalid parent: " + std::to_string(source.parent));
			}

			Joint joint;
			joint.name = text.get(source.name);
			joint.parentId = source.parent < 0 ? -1 : source.parent;
			joint.location = glm::vec3{source.translate[0], source.translate[1], source.translate[2]};
			joint.rotation = toQuat(source.rotate);

			if (joint.parentId >= 0)
			{
				const Joint &parent = model->joints[joint.parentId];
				joint.location = parent.location + parent.rotation * joint.location;
				joint.rotation = glm::normalize(parent.rotation * joint.rotation);
			}

			model->joints.push_back(joint);
		}

		// Vertex arrays
		reader.seek(header.ofs_vertexarrays);
		std::vector<float> positions, texCoords, normals, blendIndexes, blendWeights;
		for (const auto &array : reader.readArray<iqmVertexArray_t>(header.num_vertexarrays))
		{
			switch (array.type)
			{
			case IQM_POSITION: positions = readVertexArray(reader, array, 3, header.num_vertexes); break;
			case IQM_TEXCOORD: texCoords = readVertexArray(reader, array, 2, header.num_vertexes); break;
			case IQM_NORMAL: normals = readVertexArray(reader, array, 3, header.num_vertexes); break;
			case IQM_BLENDINDEXES: blendIndexes = readVertexArray(reader, array, 4, header.num_vertexes); break;
			case IQM_BLENDWEIGHTS:
				blendWeights = readVertexArray(reader, array, 4, header.num_vertexes);
				if (array.format == IQM_UBYTE)
				{
					for (auto &weight : blendWeights)
					{
						weight /= 255.0f;
					}
				}
				break;
			default: break; // tangents, colors and custom arrays are not used
			}
		}

		if (header.num_meshes > 0 && positions.empty())
		{
			throw mt_ex("The IQM file has no vertex positions");
		}

		const bool skinned = !model->joints.empty() && !blendIndexes.empty() && !blendWeights.empty();

		reader.seek(header.ofs_triangles);
		auto triangles = reader.readArray<iqmTriangle_t>(header.num_triangles);

		reader.seek(header.ofs_meshes);
		for (const auto &source : reader.readArray<iqmMesh_t>(header.num_meshes))
		{
			if (uint64_t(source.first_vertex) + source.num_vertexes > header.num_vertexes
				|| uint64_t(source.first_triangle) + source.num_triangles > header.num_triangles)
			{
				throw mt_ex("Mesh points outside the vertex or triangle data");
			}

			Mesh mesh;
			mesh.name = text.get(source.name);
			mesh.materialName = text.get(source.material);

			std::vector<glm::vec3> meshNormals;
			for (uint32_t v = source.first_vertex; v < source.first_vertex + source.num_vertexes; v++)
			{
				SkeletalVertex vertex;
				vertex.pos = {positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
				if (!texCoords.empty())
				{
					vertex.tex = {texCoords[v * 2], texCoords[v * 2 + 1]};
				}
				if (!normals.empty())
				{
					meshNormals.emplace_back(normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]);
				}

				vertex.startWeight = int(mesh.weights.size());
				if (skinned)
				{
					float total = 0;
					for (uint32_t i = 0; i < 4; i++)
					{
						float bias = blendWeights[v * 4 + i];
						if (bias <= 0.0f)
						{
							continue;
						}

						auto jointId = int(blendIndexes[v * 4 + i]);
						if (jointId < 0 || jointId >= int(model->joints.size()))
						{
							throw mt_ex("Vertex " + std::to_string(v) + " has a weight for an invalid joint: " + std::to_string(jointId));
						}

						// IQM uses matrix palette skinning, our weights are positions in the joint space
						const Joint &joint = model->joints[jointId];
						glm::vec3 local = glm::inverse(joint.rotation) * (vertex.pos - joint.location);
						mesh.weights.push_back({jointId, bias, local});
						total += bias;
						vertex.weightCount++;
					}

					// The byte weights do not always add up to exactly 1
					for (int i = 0; i < vertex.weightCount && total > 0.0f; i++)
					{
						mesh.weights[vertex.startWeight + i].bias /= total;
					}
				}

				mesh.vertices.push_back(vertex);
			}

			for (uint32_t t = source.first_triangle; t < source.first_triangle + source.num_triangles; t++)
			{
				glm::ivec3 tri;
				for (int i = 0; i < 3; i++)
				{
					uint32_t index = triangles[t].vertex[i];
					if (index < source.first_vertex || index >= source.first_vertex + source.num_vertexes)
					{
						throw mt_ex("Mesh " + mesh.name
									+ " has a triangle pointing to a vertex outside the mesh: " + std::to_string(index));
					}
					tri[i] = int(index - source.first_vertex);
				}
				mesh.tris.push_back(tri);
			}

			if (meshNormals.empty())
			{
				calculateNormals(*model, mesh, FRAME_BIND_POSE);
			}
			else
			{
				setNormals(*model, mesh, meshNormals, FRAME_BIND_POSE);
			}

			model->meshes.push_back(std::move(mesh));
		}

		// Animation frames
		if (header.num_frames > 0 && header.num_poses > 0)
		{
			if (header.num_poses != header.num_joints)
			{
				throw mt_ex("The IQM file has " + std::to_string(header.num_poses) + " poses but " + std::to_string(header.num_joints)
							+ " joints");
			}

			reader.seek(header.ofs_poses);
			auto poses = reader.readArray<iqmPose_t>(header.num_poses);
			reader.seek(header.ofs_frames);
			auto frameData = reader.readArray<uint16_t>(int64_t(header.num_frames) * header.num_framechannels);
			size_t channel = 0;

			for (uint32_t frame = 0; frame < header.num_frames; frame++)
			{
				for (size_t p = 0; p < poses.size(); p++)
				{
					const auto &pose = poses[p];
					float values[10];
					for (int c = 0; c < 10; c++)
					{
						values[c] = pose.channeloffset[c];
						if (pose.mask & (1u << unsigned(c)))
						{
							if (channel >= frameData.size())
							{
								throw mt_ex("The IQM frame data is too short");
							}
							values[c] += float(frameData[channel++]) * pose.channelscale[c];
						}
					}

					Point point;
					point.location = {values[0], values[1], values[2]};
					point.rotation = glm::normalize(glm::quat(values[6], values[3], values[4], values[5]));

					if (pose.parent >= int32_t(p))
					{
						throw mt_ex("Pose " + std::to_string(p) + " has an invalid parent: " + std::to_string(pose.parent));
					}

					if (pose.parent >= 0)
					{
						const Point &parent = model->joints[pose.parent].frames.back();
						point.location = parent.location + parent.rotation * point.location;
						point.rotation = glm::normalize(parent.rotation * point.rotation);
					}

					model->joints[p].frames.push_back(point);
				}
			}

			if (header.ofs_bounds)
			{
				reader.seek(header.ofs_bounds);
				for (const auto &bounds : reader.readArray<iqmBounds_t>(header.num_frames))
				{
					model->bounds.push_back(
						{{bounds.bbmin[0], bounds.bbmin[1], bounds.bbmin[2]}, {bounds.bbmax[0], bounds.bbmax[1], bounds.bbmax[2]}});
				}
			}
		}

		reader.seek(header.ofs_anims);
		for (const auto &source : reader.readArray<iqmAnim_t>(header.num_anims))
		{
			if (uint64_t(source.first_frame) + source.num_frames > header.num_frames)
			{
				throw mt_ex("Animation points outside the frames");
			}

			Animation animation;
			animation.name = text.get(source.name);
			animation.startFrame = int(source.first_frame);
			animation.numFrames = int(source.num_frames);
			animation.fps = int(std::lround(source.framerate));
			animation.loop = (source.flags & IQM_LOOP) != 0;
			model->animations.push_back(animation);
		}

		return model.release();
	}
}
