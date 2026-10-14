#include "model_gltf.h"
#include "image.h"
#include "measure.h"
#include "scene_import.h"

#include <tiny_gltf_v3.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

namespace mt::model {

	namespace {
		std::string str(tg3_str value)
		{
			return value.data ? std::string(value.data, value.len) : std::string();
		}

		template<typename T>
		std::vector<T> span(const T *data, uint32_t count)
		{
			return data ? std::vector<T>(data, data + count) : std::vector<T>();
		}

		/*! The parsed model, freed with the loader */
		struct Gltf : tg3_model {
			Gltf() : tg3_model{} {}
			~Gltf()
			{
				tg3_model_free(this);
			}
			Gltf(const Gltf &) = delete;
			Gltf &operator=(const Gltf &) = delete;
		};

		/**
		 * Read an accessor as floats (normalized integers are converted to 0..1 / -1..1)
		 * @return count * components values
		 */
		std::vector<float> readFloats(const tg3_model &gltf, int accessorIndex, int components)
		{
			if (accessorIndex < 0 || accessorIndex >= int(gltf.accessors_count))
			{
				throw mt_ex("Invalid accessor: " + std::to_string(accessorIndex));
			}

			const auto &accessor = gltf.accessors[accessorIndex];
			if (accessor.sparse.is_sparse)
			{
				throw mt_ex("Sparse accessors are not supported");
			}

			if (tg3_num_components(accessor.type) != components)
			{
				throw mt_ex("Unexpected accessor type for accessor " + std::to_string(accessorIndex));
			}

			if (accessor.buffer_view < 0 || accessor.buffer_view >= int(gltf.buffer_views_count))
			{
				throw mt_ex("Invalid buffer view in accessor " + std::to_string(accessorIndex));
			}

			const auto &view = gltf.buffer_views[accessor.buffer_view];
			if (view.buffer < 0 || view.buffer >= int(gltf.buffers_count))
			{
				throw mt_ex("Invalid buffer in buffer view " + std::to_string(accessor.buffer_view));
			}

			const auto &buffer = gltf.buffers[view.buffer].data;
			const int componentSize = tg3_component_size(accessor.component_type);
			if (componentSize <= 0)
			{
				throw mt_ex("Invalid component type in accessor " + std::to_string(accessorIndex));
			}

			const size_t elementSize = size_t(componentSize) * size_t(components);
			const size_t stride = view.byte_stride ? size_t(view.byte_stride) : elementSize;
			const size_t start = view.byte_offset + accessor.byte_offset;

			if (accessor.count > 0
				&& (start + stride * (accessor.count - 1) + elementSize > view.byte_offset + view.byte_length
					|| view.byte_offset + view.byte_length > buffer.count))
			{
				throw mt_ex("Accessor " + std::to_string(accessorIndex) + " points outside its buffer");
			}

			std::vector<float> out;
			out.reserve(accessor.count * size_t(components));

			for (size_t i = 0; i < accessor.count; i++)
			{
				const unsigned char *element = buffer.data + start + stride * i;
				for (int c = 0; c < components; c++)
				{
					const unsigned char *value = element + size_t(c) * size_t(componentSize);
					float result = 0;
					switch (accessor.component_type)
					{
					case TG3_COMPONENT_TYPE_FLOAT: std::memcpy(&result, value, 4); break;
					case TG3_COMPONENT_TYPE_UNSIGNED_BYTE: result = float(*value) / (accessor.normalized ? 255.0f : 1.0f); break;
					case TG3_COMPONENT_TYPE_BYTE: {
						auto signedValue = float(int8_t(*value));
						result = accessor.normalized ? std::max(signedValue / 127.0f, -1.0f) : signedValue;
						break;
					}
					case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
						uint16_t raw;
						std::memcpy(&raw, value, 2);
						result = float(raw) / (accessor.normalized ? 65535.0f : 1.0f);
						break;
					}
					case TG3_COMPONENT_TYPE_SHORT: {
						int16_t raw;
						std::memcpy(&raw, value, 2);
						result = accessor.normalized ? std::max(float(raw) / 32767.0f, -1.0f) : float(raw);
						break;
					}
					case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
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

		std::vector<uint32_t> readIndices(const tg3_model &gltf, int accessorIndex)
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

		TRS restTransform(const tg3_node &node)
		{
			TRS out;
			if (node.has_matrix)
			{
				out.hasMatrix = true;
				for (int i = 0; i < 16; i++)
				{
					out.matrix[i / 4][i % 4] = float(node.matrix[i]);
				}
				return out;
			}

			out.translation = {float(node.translation[0]), float(node.translation[1]), float(node.translation[2])};
			out.rotation = glm::normalize(
				glm::quat(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2])));
			out.scale = {float(node.scale[0]), float(node.scale[1]), float(node.scale[2])};
			return out;
		}

		class Converter {
		  public:
			Converter(const tg3_model &gltf, import::Scene &target) : gltf(gltf), target(target) {}

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
				for (uint32_t i = 0; i < gltf.nodes_count; i++)
				{
					rest.push_back(restTransform(gltf.nodes[i]));
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
				parents.assign(gltf.nodes_count, -1);
				for (size_t i = 0; i < gltf.nodes_count; i++)
				{
					for (int child : span(gltf.nodes[i].children, gltf.nodes[i].children_count))
					{
						if (child < 0 || child >= int(gltf.nodes_count) || parents[child] >= 0)
						{
							throw mt_ex("Invalid node hierarchy");
						}
						parents[child] = int(i);
					}
				}

				std::vector<int> roots;
				int sceneIndex = gltf.default_scene >= 0 ? gltf.default_scene : 0;
				if (sceneIndex < int(gltf.scenes_count))
				{
					roots = span(gltf.scenes[sceneIndex].nodes, gltf.scenes[sceneIndex].nodes_count);
				}
				else
				{
					for (size_t i = 0; i < gltf.nodes_count; i++)
					{
						if (parents[i] < 0)
						{
							roots.push_back(int(i));
						}
					}
				}

				std::vector<int> stack(roots.rbegin(), roots.rend());
				std::vector<bool> visited(gltf.nodes_count, false);
				while (!stack.empty())
				{
					int node = stack.back();
					stack.pop_back();
					if (node < 0 || node >= int(gltf.nodes_count) || visited[node])
					{
						throw mt_ex("Invalid node hierarchy");
					}
					visited[node] = true;
					order.push_back(node);

					const auto children = span(gltf.nodes[node].children, gltf.nodes[node].children_count);
					for (auto it = children.rbegin(); it != children.rend(); ++it)
					{
						stack.push_back(*it);
					}
				}
			}

			std::vector<glm::mat4> worldMatrices(const std::vector<TRS> &local) const
			{
				std::vector<glm::mat4> world(gltf.nodes_count, glm::mat4{1.0f});
				for (int node : order)
				{
					glm::mat4 matrix = local[node].toMatrix();
					world[node] = parents[node] >= 0 ? world[parents[node]] * matrix : matrix;
				}
				return world;
			}

			void collectJoints()
			{
				std::vector<bool> used(gltf.nodes_count, false);
				auto markWithParents = [&](int node) {
					for (int current = node; current >= 0; current = parents[current])
					{
						used[current] = true;
					}
				};

				for (int node : order)
				{
					const auto &source = gltf.nodes[node];
					if (import::isTagName(str(source.name)))
					{
						markWithParents(node);
					}

					if (!target.skinned)
					{
						continue;
					}

					if (source.skin >= 0)
					{
						if (source.skin >= int(gltf.skins_count))
						{
							throw mt_ex("Invalid skin: " + std::to_string(source.skin));
						}

						const auto &skin = gltf.skins[source.skin];
						for (int joint : span(skin.joints, skin.joints_count))
						{
							if (joint < 0 || joint >= int(gltf.nodes_count))
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
					joint.name = str(gltf.nodes[node].name);
					if (joint.name.empty())
					{
						joint.name = "node" + std::to_string(node);
					}
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

				// Skinned bones bind at the inverse of their inverse bind matrix. The matrices are in the space of the
				// skinned mesh's node (joint * inverse bind gives the mesh node's place at rest), and the vertices are
				// placed with that node's world matrix, so the binds get it too
				for (uint32_t s = 0; s < gltf.skins_count; s++)
				{
					const auto &skin = gltf.skins[s];
					if (skin.inverse_bind_matrices < 0)
					{
						continue;
					}

					glm::mat4 meshWorld{1.0f};
					for (int node : order)
					{
						if (gltf.nodes[node].skin == int(s) && gltf.nodes[node].mesh >= 0)
						{
							meshWorld = restWorld[node];
							break;
						}
					}

					auto matrices = readFloats(gltf, skin.inverse_bind_matrices, 16);
					if (matrices.size() < size_t(skin.joints_count) * 16)
					{
						throw mt_ex("The skin has too few inverse bind matrices");
					}

					for (size_t j = 0; j < skin.joints_count; j++)
					{
						auto found = nodeIndex.find(skin.joints[j]);
						if (found == nodeIndex.end())
						{
							continue;
						}

						glm::mat4 inverseBind = glm::make_mat4(matrices.data() + j * 16);
						target.nodes[found->second].bind = meshWorld * glm::inverse(inverseBind);
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
				for (uint32_t a = 0; a < gltf.animations_count; a++)
				{
					const auto &animation = gltf.animations[a];
					const std::string name = str(animation.name);
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

					for (uint32_t c = 0; c < animation.channels_count; c++)
					{
						const auto &channel = animation.channels[c];
						if (channel.sampler < 0 || channel.sampler >= int(animation.samplers_count) || channel.target.node < 0
							|| channel.target.node >= int(gltf.nodes_count))
						{
							throw mt_ex("Invalid animation channel in " + name);
						}

						const std::string path = str(channel.target.path);
						if (path != "translation" && path != "rotation" && path != "scale")
						{
							continue; // morph target weights are not supported
						}

						const auto &sampler = animation.samplers[channel.sampler];
						const std::string interpolation = str(sampler.interpolation);
						Channel out{
							channel.target.node,		   path, readFloats(gltf, sampler.input, 1), {}, {}, interpolation == "STEP",
							interpolation == "CUBICSPLINE"};

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
								throw mt_ex("Too few animation values in " + name);
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
								throw mt_ex("Too few animation values in " + name);
							}
						}

						duration = std::max(duration, out.times.back());
						channels.push_back(std::move(out));
					}

					import::Animation sequence;
					sequence.name = name;
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

			/*! The file data of an image: its buffer view or its data URI (the bytes as they are, PNG or JPEG) */
			std::vector<byte> imageData(int index) const
			{
				const auto &image = gltf.images[index];
				if (image.image.count > 0)
				{
					return {image.image.data, image.image.data + image.image.count};
				}
				if (image.buffer_view >= 0 && image.buffer_view < int(gltf.buffer_views_count))
				{
					const auto &view = gltf.buffer_views[image.buffer_view];
					if (view.buffer >= 0 && view.buffer < int(gltf.buffers_count))
					{
						const auto &buffer = gltf.buffers[view.buffer].data;
						if (view.byte_offset + view.byte_length <= buffer.count)
						{
							return {buffer.data + view.byte_offset, buffer.data + view.byte_offset + view.byte_length};
						}
					}
				}
				const std::string uri = str(image.uri);
				const size_t comma = uri.find(',');
				if (uri.rfind("data:", 0) == 0 && comma != std::string::npos && uri.find(";base64") < comma)
				{
					return decodeBase64(uri.substr(comma + 1));
				}
				return {};
			}

			static std::vector<byte> decodeBase64(const std::string &text)
			{
				std::vector<byte> out;
				uint32_t bits = 0;
				int count = 0;
				for (char c : text)
				{
					int value;
					if (c >= 'A' && c <= 'Z')
						value = c - 'A';
					else if (c >= 'a' && c <= 'z')
						value = c - 'a' + 26;
					else if (c >= '0' && c <= '9')
						value = c - '0' + 52;
					else if (c == '+' || c == '-')
						value = 62;
					else if (c == '/' || c == '_')
						value = 63;
					else
						continue;
					bits = (bits << 6) | uint32_t(value);
					count += 6;
					if (count >= 8)
					{
						count -= 8;
						out.push_back(byte((bits >> count) & 0xff));
					}
				}
				return out;
			}

			/*! Keep an image for a material name; another image with a name in use gets the material index added */
			std::string keepImage(std::string name, int index, std::vector<byte> data)
			{
				auto found = target.images.find(name);
				if (found != target.images.end() && found->second != data)
				{
					name += " #" + std::to_string(index);
				}
				target.images[name] = std::move(data);
				return name;
			}

			/**
			 * Name of a material for the meshes. External images keep their file name (the textures are looked up like
			 * any other). Embedded images are kept in the model under the name, tinted with the base color factor; a
			 * material with only a color gets a small image of that color, so the tool shows it.
			 */
			std::string materialName(int index)
			{
				if (index < 0 || index >= int(gltf.materials_count))
				{
					return "";
				}
				if (auto known = materialNames.find(index); known != materialNames.end())
				{
					return known->second;
				}

				const auto &material = gltf.materials[index];
				const double *factor = material.pbr_metallic_roughness.base_color_factor;
				const glm::vec4 color{float(factor[0]), float(factor[1]), float(factor[2]), float(factor[3])};
				const bool tinted = color != glm::vec4(1.0f);
				std::string name = str(material.name);

				int texture = material.pbr_metallic_roughness.base_color_texture.index;
				int imageIndex = texture >= 0 && texture < int(gltf.textures_count) ? gltf.textures[texture].source : -1;
				if (imageIndex >= 0 && imageIndex < int(gltf.images_count))
				{
					const auto &image = gltf.images[imageIndex];
					const std::string uri = str(image.uri);
					if (!uri.empty() && uri.rfind("data:", 0) != 0)
					{
						name = uri;
					}
					else
					{
						std::vector<byte> data = imageData(imageIndex);
						if (image.name.len > 0)
						{
							name = str(image.name);
						}
						if (name.empty())
						{
							name = "image " + std::to_string(imageIndex);
						}
						if (!data.empty() && tinted)
						{
							// The tool has no material colors: the color goes into the image
							mt::io::Image pixels;
							if (mt::io::decodeImage(data.data(), data.size(), pixels))
							{
								for (size_t p = 0; p + 3 < pixels.pixels.size(); p += 4)
								{
									for (int c = 0; c < 4; c++)
									{
										pixels.pixels[p + size_t(c)]
											= byte(std::lround(float(pixels.pixels[p + size_t(c)]) * glm::clamp(color[c], 0.0f, 1.0f)));
									}
								}
								data = mt::io::encodePNG(pixels);
							}
						}
						if (!data.empty())
						{
							name = keepImage(name, index, std::move(data));
						}
					}
				}
				else
				{
					// Only a color: a 4 x 4 image of it
					mt::io::Image pixels;
					pixels.width = pixels.height = 4;
					const glm::vec4 clamped = glm::clamp(color, 0.0f, 1.0f);
					for (int p = 0; p < 16; p++)
					{
						for (int c = 0; c < 4; c++)
						{
							pixels.pixels.push_back(byte(std::lround(clamped[c] * 255.0f)));
						}
					}
					if (name.empty())
					{
						char hex[16];
						std::snprintf(hex, sizeof(hex), "#%02x%02x%02x", int(std::lround(clamped.r * 255)),
									  int(std::lround(clamped.g * 255)), int(std::lround(clamped.b * 255)));
						name = std::string("color ") + hex;
					}
					name = keepImage(name, index, mt::io::encodePNG(pixels));
				}

				materialNames[index] = name;
				return name;
			}

			void addMesh(int nodeIndexValue)
			{
				const auto &node = gltf.nodes[nodeIndexValue];
				if (node.mesh >= int(gltf.meshes_count))
				{
					throw mt_ex("Invalid mesh: " + std::to_string(node.mesh));
				}

				const auto &mesh = gltf.meshes[node.mesh];
				const std::string meshName = str(mesh.name);
				const bool skinnedMesh = node.skin >= 0;
				const glm::mat4 world = restWorld[nodeIndexValue];
				const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));

				for (size_t p = 0; p < mesh.primitives_count; p++)
				{
					const auto &primitive = mesh.primitives[p];
					if (primitive.mode != TG3_MODE_TRIANGLES && primitive.mode != -1)
					{
						continue;
					}

					auto attribute = [&](const char *name) {
						for (uint32_t a = 0; a < primitive.attributes_count; a++)
						{
							if (tg3_str_equals_cstr(primitive.attributes[a].key, name))
							{
								return int(primitive.attributes[a].value);
							}
						}
						return -1;
					};

					if (attribute("POSITION") < 0)
					{
						throw mt_ex("Mesh " + meshName + " has no positions");
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
							throw mt_ex("Skinned mesh " + meshName + " has no joints or weights");
						}
						joints = readFloats(gltf, attribute("JOINTS_0"), 4);
						weights = readFloats(gltf, attribute("WEIGHTS_0"), 4);
					}

					if ((!normals.empty() && normals.size() != numVertices * 3) || (!uvs.empty() && uvs.size() != numVertices * 2)
						|| (skinnedMesh && (joints.size() != numVertices * 4 || weights.size() != numVertices * 4)))
					{
						throw mt_ex("Mesh " + meshName + " has attributes with different vertex counts");
					}

					import::Mesh out;
					out.name = meshName.empty() ? str(node.name) : meshName;
					if (mesh.primitives_count > 1)
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
							// Skinned vertices are in the bind pose in the mesh node's space: like the bones' binds, they are
							// placed with the node's world matrix (a Y-up root above the model turns them both)
							vert.pos = glm::vec3(world * glm::vec4(pos, 1.0f));
							vert.normal = normalMatrix * normal;
							const auto &skin = gltf.skins[node.skin];
							for (int i = 0; i < 4; i++)
							{
								float weight = weights[v * 4 + i];
								if (weight <= 0)
								{
									continue;
								}
								auto joint = size_t(joints[v * 4 + i]);
								if (joint >= skin.joints_count)
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
							throw mt_ex("Mesh " + meshName + " has an invalid index");
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

			const tg3_model &gltf;
			import::Scene &target;
			std::map<int, std::string> materialNames{}; ///< material index -> name, the images are made once
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

		tg3_parse_options options;
		tg3_parse_options_init(&options);
		options.images_as_is = 1; // textures are loaded by the renderer, not from the glTF

		// External buffers are refused with a clear message (the name is kept for it)
		std::string externalPath;
		options.fs.user_data = &externalPath;
		options.fs.file_exists = [](const char *, uint32_t, void *) -> int32_t { return 1; };
		options.fs.read_file = [](uint8_t **, uint64_t *, const char *path, uint32_t length, void *user) -> int32_t {
			*static_cast<std::string *>(user) = std::string(path, length);
			return 0;
		};

		const size_t length = file->len ? std::min(file->len, file->data->size()) : file->data->size();
		const auto *data = reinterpret_cast<const uint8_t *>(file->data->data());

		Gltf gltf;
		tg3_error_stack errors;
		tg3_error_stack_init(&errors);
		tg3_error_code result = tg3_parse_auto(&gltf, &errors, data, length, "", 0, &options);

		std::string message;
		for (uint32_t i = 0; i < errors.count; i++)
		{
			if (errors.entries[i].severity == TG3_SEVERITY_ERROR && errors.entries[i].message)
			{
				message += (message.empty() ? "" : "; ") + std::string(errors.entries[i].message);
			}
		}
		const bool failed = result != TG3_OK || errors.has_error;
		tg3_error_stack_free(&errors);

		if (!externalPath.empty())
		{
			throw mt_ex("External files are not supported, embed the buffers or use .glb: " + externalPath);
		}
		if (failed)
		{
			throw mt_ex("Failed to load " + file->name + ": " + message);
		}

		import::Scene scene;
		scene.name = std::filesystem::path(file->name).stem().string();
		// glTF is Y-up and in meters: converted to Z-up and game units (1 unit = 1 inch, METERS_PER_UNIT), the reverse of
		// the glTF export, so a model comes back in its size
		scene.axisConversion = glm::mat3_cast(glm::angleAxis(glm::radians(90.0f), glm::vec3{1, 0, 0}));
		scene.unitScale = 1.0f / METERS_PER_UNIT;

		Converter(gltf, scene).convert();

		if (scene.meshes.empty())
		{
			throw mt_ex("The file has no meshes: " + file->name);
		}

		return import::toModel(std::move(scene));
	}
}
