#include "model_gltf.h"
#include "scene_import.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

#include <cstring>
#include <filesystem>
#include <map>
#include <tuple>

namespace mt::model {

	namespace {
		/**
		 * Read an accessor as floats (normalized integers are converted to 0..1 / -1..1)
		 * @return count * components values
		 */
		std::vector<float> readFloats(const tinygltf::Model &gltf, int accessorIndex, int components)
		{
			if (accessorIndex < 0 || accessorIndex >= int(gltf.accessors.size()))
			{
				throw mt_ex("Invalid accessor: " + std::to_string(accessorIndex));
			}

			const auto &accessor = gltf.accessors[accessorIndex];
			if (accessor.sparse.isSparse)
			{
				throw mt_ex("Sparse accessors are not supported");
			}

			if (tinygltf::GetNumComponentsInType(uint32_t(accessor.type)) != components)
			{
				throw mt_ex("Unexpected accessor type for accessor " + std::to_string(accessorIndex));
			}

			if (accessor.bufferView < 0 || accessor.bufferView >= int(gltf.bufferViews.size()))
			{
				throw mt_ex("Invalid buffer view in accessor " + std::to_string(accessorIndex));
			}

			const auto &view = gltf.bufferViews[accessor.bufferView];
			if (view.buffer < 0 || view.buffer >= int(gltf.buffers.size()))
			{
				throw mt_ex("Invalid buffer in buffer view " + std::to_string(accessor.bufferView));
			}

			const auto &buffer = gltf.buffers[view.buffer].data;
			const int componentSize = tinygltf::GetComponentSizeInBytes(uint32_t(accessor.componentType));
			if (componentSize <= 0)
			{
				throw mt_ex("Invalid component type in accessor " + std::to_string(accessorIndex));
			}

			const size_t elementSize = size_t(componentSize) * size_t(components);
			const size_t stride = view.byteStride ? view.byteStride : elementSize;
			const size_t start = view.byteOffset + accessor.byteOffset;

			if (accessor.count > 0
				&& (start + stride * (accessor.count - 1) + elementSize > view.byteOffset + view.byteLength
					|| view.byteOffset + view.byteLength > buffer.size()))
			{
				throw mt_ex("Accessor " + std::to_string(accessorIndex) + " points outside its buffer");
			}

			std::vector<float> out;
			out.reserve(accessor.count * size_t(components));

			for (size_t i = 0; i < accessor.count; i++)
			{
				const unsigned char *element = buffer.data() + start + stride * i;
				for (int c = 0; c < components; c++)
				{
					const unsigned char *value = element + size_t(c) * size_t(componentSize);
					float result = 0;
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_FLOAT: std::memcpy(&result, value, 4); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: result = float(*value) / (accessor.normalized ? 255.0f : 1.0f); break;
					case TINYGLTF_COMPONENT_TYPE_BYTE: {
						auto signedValue = float(int8_t(*value));
						result = accessor.normalized ? std::max(signedValue / 127.0f, -1.0f) : signedValue;
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
						uint16_t raw;
						std::memcpy(&raw, value, 2);
						result = float(raw) / (accessor.normalized ? 65535.0f : 1.0f);
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_SHORT: {
						int16_t raw;
						std::memcpy(&raw, value, 2);
						result = accessor.normalized ? std::max(float(raw) / 32767.0f, -1.0f) : float(raw);
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
						uint32_t raw;
						std::memcpy(&raw, value, 4);
						result = float(raw);
						break;
					}
					default: throw mt_ex("Unsupported component type in accessor " + std::to_string(accessorIndex));
					}
					out.push_back(result);
				}
			}

			return out;
		}

		std::vector<uint32_t> readIndices(const tinygltf::Model &gltf, int accessorIndex)
		{
			auto values = readFloats(gltf, accessorIndex, 1);
			std::vector<uint32_t> out(values.size());
			std::transform(values.begin(), values.end(), out.begin(), [](float value) { return uint32_t(value); });
			return out;
		}

		struct TRS {
			glm::vec3 translation{0};
			glm::quat rotation{1, 0, 0, 0};
			glm::vec3 scale{1};
			bool hasMatrix = false;
			glm::mat4 matrix{1.0f};

			[[nodiscard]] glm::mat4 toMatrix() const
			{
				if (hasMatrix)
				{
					return matrix;
				}
				return glm::translate(translation) * glm::toMat4(rotation) * glm::scale(scale);
			}
		};

		TRS restTransform(const tinygltf::Node &node)
		{
			TRS out;
			if (node.matrix.size() == 16)
			{
				out.hasMatrix = true;
				for (int i = 0; i < 16; i++)
				{
					out.matrix[i / 4][i % 4] = float(node.matrix[i]);
				}
				return out;
			}

			if (node.translation.size() == 3)
			{
				out.translation = {float(node.translation[0]), float(node.translation[1]), float(node.translation[2])};
			}
			if (node.rotation.size() == 4)
			{
				out.rotation = glm::normalize(
					glm::quat(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2])));
			}
			if (node.scale.size() == 3)
			{
				out.scale = {float(node.scale[0]), float(node.scale[1]), float(node.scale[2])};
			}
			return out;
		}

		class Converter {
		  public:
			Converter(const tinygltf::Model &gltf, import::Scene &target) : gltf(gltf), target(target) {}

			void convert()
			{
				collectOrder();

				target.skinned = false;
				for (int node : order)
				{
					const auto &source = gltf.nodes[node];
					if (source.mesh >= 0 && source.skin >= 0)
					{
						target.skinned = true;
					}
				}

				std::vector<TRS> rest;
				for (const auto &node : gltf.nodes)
				{
					rest.push_back(restTransform(node));
				}
				restWorld = worldMatrices(rest);

				collectJoints();
				sampleAnimations(rest);

				for (int node : order)
				{
					if (gltf.nodes[node].mesh >= 0)
					{
						addMesh(node);
					}
				}
			}

		  private:
			/*! Nodes of the scene, parents first */
			void collectOrder()
			{
				parents.assign(gltf.nodes.size(), -1);
				for (size_t i = 0; i < gltf.nodes.size(); i++)
				{
					for (int child : gltf.nodes[i].children)
					{
						if (child < 0 || child >= int(gltf.nodes.size()) || parents[child] >= 0)
						{
							throw mt_ex("Invalid node hierarchy");
						}
						parents[child] = int(i);
					}
				}

				std::vector<int> roots;
				int sceneIndex = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
				if (sceneIndex < int(gltf.scenes.size()))
				{
					roots = gltf.scenes[sceneIndex].nodes;
				}
				else
				{
					for (size_t i = 0; i < gltf.nodes.size(); i++)
					{
						if (parents[i] < 0)
						{
							roots.push_back(int(i));
						}
					}
				}

				std::vector<int> stack(roots.rbegin(), roots.rend());
				std::vector<bool> visited(gltf.nodes.size(), false);
				while (!stack.empty())
				{
					int node = stack.back();
					stack.pop_back();
					if (node < 0 || node >= int(gltf.nodes.size()) || visited[node])
					{
						throw mt_ex("Invalid node hierarchy");
					}
					visited[node] = true;
					order.push_back(node);

					const auto &children = gltf.nodes[node].children;
					for (auto it = children.rbegin(); it != children.rend(); ++it)
					{
						stack.push_back(*it);
					}
				}
			}

			std::vector<glm::mat4> worldMatrices(const std::vector<TRS> &local) const
			{
				std::vector<glm::mat4> world(gltf.nodes.size(), glm::mat4{1.0f});
				for (int node : order)
				{
					glm::mat4 matrix = local[node].toMatrix();
					world[node] = parents[node] >= 0 ? world[parents[node]] * matrix : matrix;
				}
				return world;
			}

			void collectJoints()
			{
				std::vector<bool> used(gltf.nodes.size(), false);
				auto markWithParents = [&](int node) {
					for (int current = node; current >= 0; current = parents[current])
					{
						used[current] = true;
					}
				};

				for (int node : order)
				{
					const auto &source = gltf.nodes[node];
					if (import::isTagName(source.name))
					{
						markWithParents(node);
					}

					if (!target.skinned)
					{
						continue;
					}

					if (source.skin >= 0)
					{
						if (source.skin >= int(gltf.skins.size()))
						{
							throw mt_ex("Invalid skin: " + std::to_string(source.skin));
						}

						for (int joint : gltf.skins[source.skin].joints)
						{
							if (joint < 0 || joint >= int(gltf.nodes.size()))
							{
								throw mt_ex("Invalid skin joint: " + std::to_string(joint));
							}
							markWithParents(joint);
						}
					}
					else if (source.mesh >= 0)
					{
						markWithParents(node);
					}
				}

				for (int node : order)
				{
					if (!used[node])
					{
						continue;
					}

					import::Node joint;
					joint.name = gltf.nodes[node].name.empty() ? "node" + std::to_string(node) : gltf.nodes[node].name;
					joint.bind = restWorld[node];
					for (int parent = parents[node]; parent >= 0; parent = parents[parent])
					{
						auto found = nodeIndex.find(parent);
						if (found != nodeIndex.end())
						{
							joint.parent = found->second;
							break;
						}
					}

					nodeIndex[node] = int(target.nodes.size());
					if (import::isTagName(joint.name))
					{
						target.tags.push_back(int(target.nodes.size()));
					}
					target.nodes.push_back(joint);
				}

				// Skinned bones bind at the inverse of their inverse bind matrix
				for (const auto &skin : gltf.skins)
				{
					if (skin.inverseBindMatrices < 0)
					{
						continue;
					}

					auto matrices = readFloats(gltf, skin.inverseBindMatrices, 16);
					if (matrices.size() < skin.joints.size() * 16)
					{
						throw mt_ex("The skin has too few inverse bind matrices");
					}

					for (size_t j = 0; j < skin.joints.size(); j++)
					{
						auto found = nodeIndex.find(skin.joints[j]);
						if (found == nodeIndex.end())
						{
							continue;
						}

						glm::mat4 inverseBind = glm::make_mat4(matrices.data() + j * 16);
						target.nodes[found->second].bind = glm::inverse(inverseBind);
					}
				}
			}

			template<typename T>
			static T sample(const std::vector<float> &times, const std::vector<T> &values, float time, bool step, bool cubic)
			{
				// Cubic spline samplers store in-tangent, value, out-tangent for every key, only the values are used
				auto value = [&](size_t key) -> const T & { return values.at(cubic ? key * 3 + 1 : key); };

				if (time <= times.front())
				{
					return value(0);
				}
				if (time >= times.back())
				{
					return value(times.size() - 1);
				}

				auto next = size_t(std::upper_bound(times.begin(), times.end(), time) - times.begin());
				size_t prev = next - 1;
				if (step)
				{
					return value(prev);
				}

				float t = (time - times[prev]) / (times[next] - times[prev]);
				if constexpr (std::is_same_v<T, glm::quat>)
				{
					return glm::normalize(glm::slerp(value(prev), value(next), t));
				}
				else
				{
					return glm::mix(value(prev), value(next), t);
				}
			}

			void sampleAnimations(const std::vector<TRS> &rest)
			{
				for (const auto &animation : gltf.animations)
				{
					struct Channel {
						int node;
						std::string path;
						std::vector<float> times;
						std::vector<glm::vec3> vectors;
						std::vector<glm::quat> rotations;
						bool step;
						bool cubic;
					};

					std::vector<Channel> channels;
					float duration = 0;

					for (const auto &channel : animation.channels)
					{
						if (channel.sampler < 0 || channel.sampler >= int(animation.samplers.size()) || channel.target_node < 0
							|| channel.target_node >= int(gltf.nodes.size()))
						{
							throw mt_ex("Invalid animation channel in " + animation.name);
						}

						if (channel.target_path != "translation" && channel.target_path != "rotation" && channel.target_path != "scale")
						{
							continue; // morph target weights are not supported
						}

						const auto &sampler = animation.samplers[channel.sampler];
						Channel out{channel.target_node,
									channel.target_path,
									readFloats(gltf, sampler.input, 1),
									{},
									{},
									sampler.interpolation == "STEP",
									sampler.interpolation == "CUBICSPLINE"};

						if (out.times.empty())
						{
							continue;
						}

						const size_t expected = out.times.size() * (out.cubic ? 3 : 1);
						if (out.path == "rotation")
						{
							auto values = readFloats(gltf, sampler.output, 4);
							for (size_t i = 0; i + 3 < values.size(); i += 4)
							{
								out.rotations.emplace_back(values[i + 3], values[i], values[i + 1], values[i + 2]);
							}
							if (out.rotations.size() < expected)
							{
								throw mt_ex("Too few animation values in " + animation.name);
							}
						}
						else
						{
							auto values = readFloats(gltf, sampler.output, 3);
							for (size_t i = 0; i + 2 < values.size(); i += 3)
							{
								out.vectors.emplace_back(values[i], values[i + 1], values[i + 2]);
							}
							if (out.vectors.size() < expected)
							{
								throw mt_ex("Too few animation values in " + animation.name);
							}
						}

						duration = std::max(duration, out.times.back());
						channels.push_back(std::move(out));
					}

					import::Animation sequence;
					sequence.name = animation.name;
					sequence.startFrame = target.numFrames;
					sequence.numFrames = int(std::lround(duration * import::DEFAULT_FPS)) + 1;
					sequence.fps = import::DEFAULT_FPS;

					for (int frame = 0; frame < sequence.numFrames; frame++)
					{
						const float time = float(frame) / float(import::DEFAULT_FPS);
						std::vector<TRS> local = rest;

						for (const auto &channel : channels)
						{
							TRS &trs = local[channel.node];
							trs.hasMatrix = false; // animated nodes use TRS
							if (channel.path == "translation")
							{
								trs.translation = sample(channel.times, channel.vectors, time, channel.step, channel.cubic);
							}
							else if (channel.path == "rotation")
							{
								trs.rotation = glm::normalize(sample(channel.times, channel.rotations, time, channel.step, channel.cubic));
							}
							else
							{
								trs.scale = sample(channel.times, channel.vectors, time, channel.step, channel.cubic);
							}
						}

						auto world = worldMatrices(local);
						for (const auto &[node, index] : nodeIndex)
						{
							target.nodes[index].frames.push_back(world[node]);
						}
						frameWorld.push_back(std::move(world));
					}

					target.numFrames += sequence.numFrames;
					target.animations.push_back(sequence);
				}
			}

			std::string materialName(int index) const
			{
				if (index < 0 || index >= int(gltf.materials.size()))
				{
					return "";
				}

				const auto &material = gltf.materials[index];
				int texture = material.pbrMetallicRoughness.baseColorTexture.index;
				if (texture >= 0 && texture < int(gltf.textures.size()))
				{
					int image = gltf.textures[texture].source;
					if (image >= 0 && image < int(gltf.images.size()))
					{
						const std::string &uri = gltf.images[image].uri;
						if (!uri.empty() && uri.rfind("data:", 0) != 0)
						{
							return uri;
						}
						if (!gltf.images[image].name.empty())
						{
							return gltf.images[image].name;
						}
					}
				}

				return material.name;
			}

			void addMesh(int nodeIndexValue)
			{
				const auto &node = gltf.nodes[nodeIndexValue];
				if (node.mesh >= int(gltf.meshes.size()))
				{
					throw mt_ex("Invalid mesh: " + std::to_string(node.mesh));
				}

				const auto &mesh = gltf.meshes[node.mesh];
				const bool skinnedMesh = node.skin >= 0;
				const glm::mat4 world = restWorld[nodeIndexValue];
				const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));

				for (size_t p = 0; p < mesh.primitives.size(); p++)
				{
					const auto &primitive = mesh.primitives[p];
					if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
					{
						continue;
					}

					auto attribute = [&](const char *name) {
						auto found = primitive.attributes.find(name);
						return found == primitive.attributes.end() ? -1 : found->second;
					};

					if (attribute("POSITION") < 0)
					{
						throw mt_ex("Mesh " + mesh.name + " has no positions");
					}

					auto positions = readFloats(gltf, attribute("POSITION"), 3);
					const size_t numVertices = positions.size() / 3;
					std::vector<float> normals = attribute("NORMAL") >= 0 ? readFloats(gltf, attribute("NORMAL"), 3) : std::vector<float>();
					std::vector<float> uvs
						= attribute("TEXCOORD_0") >= 0 ? readFloats(gltf, attribute("TEXCOORD_0"), 2) : std::vector<float>();
					std::vector<float> joints, weights;
					if (skinnedMesh)
					{
						if (attribute("JOINTS_0") < 0 || attribute("WEIGHTS_0") < 0)
						{
							throw mt_ex("Skinned mesh " + mesh.name + " has no joints or weights");
						}
						joints = readFloats(gltf, attribute("JOINTS_0"), 4);
						weights = readFloats(gltf, attribute("WEIGHTS_0"), 4);
					}

					if ((!normals.empty() && normals.size() != numVertices * 3) || (!uvs.empty() && uvs.size() != numVertices * 2)
						|| (skinnedMesh && (joints.size() != numVertices * 4 || weights.size() != numVertices * 4)))
					{
						throw mt_ex("Mesh " + mesh.name + " has attributes with different vertex counts");
					}

					import::Mesh out;
					out.name = mesh.name.empty() ? node.name : mesh.name;
					if (mesh.primitives.size() > 1)
					{
						out.name += "_" + std::to_string(p);
					}
					out.material = materialName(primitive.material);

					for (size_t v = 0; v < numVertices; v++)
					{
						import::Vertex vert;
						glm::vec3 pos{positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
						glm::vec3 normal
							= normals.empty() ? glm::vec3{0} : glm::vec3{normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]};
						if (!uvs.empty())
						{
							vert.tex = {uvs[v * 2], uvs[v * 2 + 1]};
						}

						if (skinnedMesh)
						{
							// Skinned vertices are in the bind pose already (the mesh node transform is not used)
							vert.pos = pos;
							vert.normal = normal;
							const auto &skin = gltf.skins[node.skin];
							for (int i = 0; i < 4; i++)
							{
								float weight = weights[v * 4 + i];
								if (weight <= 0)
								{
									continue;
								}
								auto joint = size_t(joints[v * 4 + i]);
								if (joint >= skin.joints.size())
								{
									throw mt_ex("Vertex has an invalid joint: " + std::to_string(joint));
								}
								vert.influences.push_back({nodeIndex.at(skin.joints[joint]), weight});
							}
						}
						else
						{
							vert.pos = glm::vec3(world * glm::vec4(pos, 1.0f));
							vert.normal = normalMatrix * normal;
							if (target.skinned)
							{
								vert.influences.push_back({nodeIndex.at(nodeIndexValue), 1.0f});
							}
						}

						out.vertices.push_back(vert);
					}

					std::vector<uint32_t> indices;
					if (primitive.indices >= 0)
					{
						indices = readIndices(gltf, primitive.indices);
					}
					else
					{
						indices.resize(numVertices);
						std::iota(indices.begin(), indices.end(), 0);
					}

					for (size_t i = 0; i + 2 < indices.size(); i += 3)
					{
						if (indices[i] >= numVertices || indices[i + 1] >= numVertices || indices[i + 2] >= numVertices)
						{
							throw mt_ex("Mesh " + mesh.name + " has an invalid index");
						}
						// glTF triangles are counter-clockwise, idTech is clockwise
						out.tris.emplace_back(int(indices[i]), int(indices[i + 2]), int(indices[i + 1]));
					}

					if (!target.skinned)
					{
						for (const auto &frame : frameWorld)
						{
							const glm::mat4 &frameMatrix = frame[nodeIndexValue];
							const glm::mat3 frameNormal = glm::transpose(glm::inverse(glm::mat3(frameMatrix)));
							std::vector<glm::vec3> framePositions, frameNormals;
							for (size_t v = 0; v < numVertices; v++)
							{
								glm::vec3 pos{positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
								glm::vec3 normal
									= normals.empty() ? glm::vec3{0} : glm::vec3{normals[v * 3], normals[v * 3 + 1], normals[v * 3 + 2]};
								framePositions.emplace_back(frameMatrix * glm::vec4(pos, 1.0f));
								frameNormals.push_back(frameNormal * normal);
							}
							out.framePositions.push_back(std::move(framePositions));
							out.frameNormals.push_back(std::move(frameNormals));
						}
					}

					target.meshes.push_back(std::move(out));
				}
			}

			const tinygltf::Model &gltf;
			import::Scene &target;
			std::vector<int> parents{};
			std::vector<int> order{};
			std::vector<glm::mat4> restWorld{};
			std::vector<std::vector<glm::mat4>> frameWorld{}; ///< world matrix of every gltf node per frame
			std::map<int, int> nodeIndex{};					  ///< gltf node -> scene node
		};
	}

	Model *loadGLTF(const Ref<mt::io::MTFile> &file)
	{
		if (!file || !file->valid())
		{
			throw mt_ex("File not found");
		}

		tinygltf::TinyGLTF loader;
		loader.SetImageLoader(
			[](tinygltf::Image *, const int, std::string *, std::string *, int, int, const unsigned char *, int, void *) { return true; },
			nullptr);

		tinygltf::FsCallbacks callbacks{};
		// Report every file as existing, so tinygltf asks ReadWholeFile and the user gets a clear message
		callbacks.FileExists = [](const std::string &, void *) { return true; };
		callbacks.ExpandFilePath = [](const std::string &path, void *) { return path; };
		auto externalFile = [](std::string *err, const std::string &path) {
			if (err)
			{
				*err = "External files are not supported, embed the buffers or use .glb: " + path;
			}
			return false;
		};
		callbacks.ReadWholeFile = [externalFile](std::vector<unsigned char> *, std::string *err, const std::string &path, void *) {
			return externalFile(err, path);
		};
		callbacks.GetFileSizeInBytes
			= [externalFile](size_t *, std::string *err, const std::string &path, void *) { return externalFile(err, path); };
		callbacks.WriteWholeFile = [](std::string *, const std::string &, const std::vector<unsigned char> &, void *) { return false; };
		loader.SetFsCallbacks(callbacks);

		const size_t length = file->len ? std::min(file->len, file->data->size()) : file->data->size();
		const unsigned char *data = file->data->data();

		tinygltf::Model gltf;
		std::string err, warn;
		bool ok;
		if (length >= 4 && std::memcmp(data, "glTF", 4) == 0)
		{
			ok = loader.LoadBinaryFromMemory(&gltf, &err, &warn, data, unsigned(length), "");
		}
		else
		{
			ok = loader.LoadASCIIFromString(&gltf, &err, &warn, reinterpret_cast<const char *>(data), unsigned(length), "");
		}

		if (!ok)
		{
			throw mt_ex("Failed to load " + file->name + ": " + err);
		}

		import::Scene scene;
		scene.name = std::filesystem::path(file->name).stem().string();
		// glTF is Y-up. The coordinates are kept as they are, like the other formats (one meter is one game unit).
		scene.axisConversion = glm::mat3_cast(glm::angleAxis(glm::radians(90.0f), glm::vec3{1, 0, 0}));

		Converter(gltf, scene).convert();

		if (scene.meshes.empty())
		{
			throw mt_ex("The file has no meshes: " + file->name);
		}

		return import::toModel(std::move(scene));
	}
}
