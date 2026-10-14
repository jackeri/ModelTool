#include "model_gltf_write.h"
#include "image.h"
#include "morph_model.h"
#include "shader_script.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "skin.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>

namespace mt::model {
	using morph::MorphModel;
	using skeletal::SkeletalModel;

	namespace {
		constexpr int GL_FLOAT_TYPE = 5126;
		constexpr int GL_UNSIGNED_SHORT_TYPE = 5123;
		constexpr int GL_UNSIGNED_INT_TYPE = 5125;
		constexpr int GL_ARRAY_BUFFER_TARGET = 34962;
		constexpr int GL_ELEMENT_ARRAY_BUFFER_TARGET = 34963;
		constexpr int GL_CLAMP_TO_EDGE_WRAP = 33071;
		constexpr int GL_REPEAT_WRAP = 10497;
		constexpr float DEFAULT_FPS = 15.0f;

		std::string jsonString(const std::string &text)
		{
			std::string out = "\"";
			for (unsigned char c : text)
			{
				switch (c)
				{
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (c < 0x20)
					{
						char code[8];
						std::snprintf(code, sizeof(code), "\\u%04x", c);
						out += code;
					}
					else
					{
						out += char(c);
					}
				}
			}
			return out + "\"";
		}

		std::string number(double value)
		{
			if (!std::isfinite(value))
			{
				return "0";
			}
			char text[32];
			std::snprintf(text, sizeof(text), "%.9g", value);
			return text;
		}

		template<typename T>
		std::string list(const T &values)
		{
			std::string out = "[";
			for (size_t i = 0; i < values.size(); i++)
			{
				out += (i ? "," : "") + number(double(values[i]));
			}
			return out + "]";
		}

		std::string joined(const std::vector<std::string> &items)
		{
			std::string out = "[";
			for (size_t i = 0; i < items.size(); i++)
			{
				out += (i ? "," : "") + items[i];
			}
			return out + "]";
		}

		std::string base64(const std::vector<byte> &data)
		{
			static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string out;
			out.reserve((data.size() + 2) / 3 * 4);
			for (size_t i = 0; i < data.size(); i += 3)
			{
				const uint32_t a = data[i];
				const uint32_t b = i + 1 < data.size() ? data[i + 1] : 0;
				const uint32_t c = i + 2 < data.size() ? data[i + 2] : 0;
				const uint32_t triple = (a << 16) | (b << 8) | c;
				out += alphabet[(triple >> 18) & 63];
				out += alphabet[(triple >> 12) & 63];
				out += i + 1 < data.size() ? alphabet[(triple >> 6) & 63] : '=';
				out += i + 2 < data.size() ? alphabet[triple & 63] : '=';
			}
			return out;
		}

		/*! A node of the glTF scene */
		struct Node {
			std::string name;
			glm::vec3 translation{0.0f};
			glm::quat rotation{1, 0, 0, 0};
			glm::vec3 scale{1.0f};
			int mesh = -1;
			int skin = -1;
			std::vector<int> children;
			std::vector<float> weights;
		};

		/*! An animation channel: the node, the path and its keyframes */
		struct Channel {
			int node;
			std::string path; ///< translation, rotation or weights
			std::vector<float> times;
			std::vector<float> values;
		};

		/*! The nodes of a part that other parts can hang on, by lowercase name */
		struct PartNodes {
			int node = -1;
			std::map<std::string, int> attach{};
		};

		class Writer {
		  public:
			Writer(const std::vector<GltfPart> &parts, const GltfOptions &options) : parts(parts), options(options)
			{
				if (options.shaders)
				{
					shaders = options.shaders;
				}
			}

			std::vector<byte> write()
			{
				if (parts.empty() || !parts[0].model)
				{
					throw mt_ex("Nothing to export");
				}

				// The root turns Z-up into Y-up (the reverse of the import's +90 degrees around X) and scales to meters
				Node root;
				root.name = "root";
				// Every place and rotation is converted to Y-up and meters itself (see point()), the root has no transform:
				// a rotation or scale on it turns the local axes of the bones and tags when importers convert to Z-up
				scale = options.scale > 0.0f ? options.scale : 1.0f;
				nodes.push_back(root);

				std::vector<PartNodes> placed;
				for (size_t p = 0; p < parts.size(); p++)
				{
					const GltfPart &part = parts[p];
					if (!part.model)
					{
						throw mt_ex("Part " + std::to_string(p) + " has no model");
					}

					int parentNode = 0;
					if (part.parent >= 0)
					{
						if (part.parent >= int(p))
						{
							throw mt_ex("Part " + part.name + " is attached to a part after it");
						}
						const PartNodes &parent = placed[size_t(part.parent)];
						auto found = parent.attach.find(tools::lowercase(part.attachPoint));
						if (found != parent.attach.end())
						{
							parentNode = found->second;
						}
						else
						{
							parentNode = parent.node;
							warn("No tag or bone " + part.attachPoint + " for " + part.name + ", it is placed on its parent's origin");
						}
					}

					placed.push_back(addPart(part, p, parentNode));
				}

				return finish();
			}

		  private:
			const std::vector<GltfPart> &parts;
			const GltfOptions &options;
			ShaderLibrary noShaders{};
			float scale = 1.0f; ///< meters per game unit, applied to every length
			/*! idTech Z-up to glTF Y-up: the inverse of the import's +90 degrees around X */
			const glm::quat toYUp = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));

			[[nodiscard]] glm::vec3 position(const glm::vec3 &pos) const
			{
				return toYUp * pos * scale;
			}

			[[nodiscard]] glm::vec3 direction(const glm::vec3 &dir) const
			{
				return toYUp * dir;
			}

			/**
			 * A place and rotation in glTF's space. The rotation is changed like a change of basis (C q C^-1), the way
			 * importers (ours, Blender) turn it back, so the local axes of bones and tags come back as they were
			 */
			[[nodiscard]] Point point(const Point &source) const
			{
				return {position(source.location), glm::normalize(toYUp * source.rotation * glm::inverse(toYUp))};
			}
			const ShaderLibrary *shaders = &noShaders;

			std::vector<Node> nodes;
			std::vector<byte> buffer;
			std::vector<std::string> bufferViews, accessors, meshes, materials, textures, images, samplers, skins;
			std::map<std::string, int> materialIndex;
			std::map<std::string, int> textureIndex;				///< by image path
			std::map<std::string, std::vector<Channel>> animations; ///< by animation name, in the order they are made
			std::vector<std::string> animationOrder;

			void warn(const std::string &message) const
			{
				if (options.warn)
				{
					options.warn(message);
				}
			}

			int addNode(Node node, int parent)
			{
				nodes.push_back(std::move(node));
				const int index = int(nodes.size()) - 1;
				if (parent >= 0)
				{
					nodes[size_t(parent)].children.push_back(index);
				}
				return index;
			}

			/*! Append data to the buffer as a buffer view, aligned to 4 bytes */
			int addView(const void *data, size_t bytes, int target)
			{
				while (buffer.size() % 4)
				{
					buffer.push_back(0);
				}
				const size_t offset = buffer.size();
				buffer.insert(buffer.end(), static_cast<const byte *>(data), static_cast<const byte *>(data) + bytes);
				std::string view = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(bytes);
				if (target)
				{
					view += ",\"target\":" + std::to_string(target);
				}
				bufferViews.push_back(view + "}");
				return int(bufferViews.size()) - 1;
			}

			int addFloats(const std::vector<float> &values, int components, const char *type, int target, bool bounds)
			{
				const int view = addView(values.data(), values.size() * sizeof(float), target);
				const size_t count = components ? values.size() / size_t(components) : 0;
				std::string accessor = "{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" + std::to_string(GL_FLOAT_TYPE)
									   + ",\"count\":" + std::to_string(count) + ",\"type\":" + jsonString(type);
				if (bounds && count > 0)
				{
					std::vector<float> low(size_t(components), std::numeric_limits<float>::max());
					std::vector<float> high(size_t(components), -std::numeric_limits<float>::max());
					for (size_t i = 0; i < values.size(); i++)
					{
						low[i % size_t(components)] = std::min(low[i % size_t(components)], values[i]);
						high[i % size_t(components)] = std::max(high[i % size_t(components)], values[i]);
					}
					accessor += ",\"min\":" + list(low) + ",\"max\":" + list(high);
				}
				accessors.push_back(accessor + "}");
				return int(accessors.size()) - 1;
			}

			int addIndices(const std::vector<glm::ivec3> &tris)
			{
				// glTF fronts are counter-clockwise, idTech fronts clockwise
				std::vector<uint32_t> indices;
				indices.reserve(tris.size() * 3);
				for (const auto &tri : tris)
				{
					indices.insert(indices.end(), {uint32_t(tri.x), uint32_t(tri.z), uint32_t(tri.y)});
				}
				const int view = addView(indices.data(), indices.size() * sizeof(uint32_t), GL_ELEMENT_ARRAY_BUFFER_TARGET);
				accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" + std::to_string(GL_UNSIGNED_INT_TYPE)
									+ ",\"count\":" + std::to_string(indices.size()) + ",\"type\":\"SCALAR\"}");
				return int(accessors.size()) - 1;
			}

			int addJoints(const std::vector<uint16_t> &joints)
			{
				const int view = addView(joints.data(), joints.size() * sizeof(uint16_t), GL_ARRAY_BUFFER_TARGET);
				accessors.push_back("{\"bufferView\":" + std::to_string(view)
									+ ",\"componentType\":" + std::to_string(GL_UNSIGNED_SHORT_TYPE)
									+ ",\"count\":" + std::to_string(joints.size() / 4) + ",\"type\":\"VEC4\"}");
				return int(accessors.size()) - 1;
			}

			/*! The material of a mesh: the skin's shader, else the mesh material; textures embedded when files are given */
			int material(const GltfPart &part, const std::string &meshName, const std::string &meshMaterial)
			{
				std::string name = meshMaterial;
				if (part.skin)
				{
					const std::string skinned = part.skin->shader(meshName);
					name = skinned.empty() ? name : skinned;
				}
				// Shared by the parts in one folder (the textures are looked up next to the models too)
				const std::string key = name + "|" + std::filesystem::path(part.path).parent_path().generic_string();
				auto found = materialIndex.find(key);
				if (found != materialIndex.end())
				{
					return found->second;
				}

				const ShaderDef *shader = shaders->find(name);
				std::string json
					= "{\"name\":" + jsonString(name) + ",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";
				const bool carried = part.model->images.count(name) > 0;
				const int texture = (options.files || carried) && !name.empty() ? addTexture(part, name, shader) : -1;
				if (texture >= 0)
				{
					json += ",\"baseColorTexture\":{\"index\":" + std::to_string(texture) + "}";
				}
				json += "}";
				if (shader && shader->cull == ShaderCull::None)
				{
					json += ",\"doubleSided\":true";
				}
				if (shader && shader->blend)
				{
					json += ",\"alphaMode\":\"BLEND\"";
				}
				else if (shader && shader->alphaFunc != AlphaFunc::None)
				{
					json += std::string(",\"alphaMode\":\"MASK\",\"alphaCutoff\":")
							+ (shader->alphaFunc == AlphaFunc::GT0 ? "0.004" : "0.5");
				}
				materials.push_back(json + "}");
				const int index = int(materials.size()) - 1;
				materialIndex[key] = index;
				return index;
			}

			int addTexture(const GltfPart &part, const std::string &material, const ShaderDef *shader)
			{
				// An image carried in the model (from a glTF file) comes first
				if (auto carried = part.model->images.find(material); carried != part.model->images.end())
				{
					return addImage(carried->second.data(), carried->second.size(), material, shader,
									"embedded:" + part.path + "|" + material);
				}
				if (!options.files)
				{
					return -1;
				}
				const std::string path = findMaterialTexture(*options.files, *shaders, material, part.path);
				auto file = path.empty() ? nullptr : options.files->loadFile(path);
				if (!file || !file->valid())
				{
					warn("No texture for " + material);
					return -1;
				}
				const size_t length = file->len ? std::min(file->len, file->data->size()) : file->data->size();
				return addImage(file->data->data(), length, path, shader, path);
			}

			/*! Embed an image file once (by key): PNG and JPEG as they are, other formats (TGA, ...) converted to PNG */
			int addImage(const byte *data, size_t length, const std::string &name, const ShaderDef *shader, const std::string &key)
			{
				if (auto known = textureIndex.find(key); known != textureIndex.end())
				{
					return known->second;
				}

				std::string mime = mt::io::imageMimeType(data, length);
				std::vector<byte> bytes;
				if (!mime.empty())
				{
					bytes.assign(data, data + length);
				}
				else
				{
					mt::io::Image image;
					if (!mt::io::decodeImage(data, length, image))
					{
						warn("Could not decode " + name);
						return -1;
					}
					bytes = mt::io::encodePNG(image);
					mime = "image/png";
				}

				const int view = addView(bytes.data(), bytes.size(), 0);
				images.push_back("{\"name\":" + jsonString(name) + ",\"bufferView\":" + std::to_string(view)
								 + ",\"mimeType\":" + jsonString(mime) + "}");
				const int wrap = shader && shader->clamp ? GL_CLAMP_TO_EDGE_WRAP : GL_REPEAT_WRAP;
				samplers.push_back("{\"wrapS\":" + std::to_string(wrap) + ",\"wrapT\":" + std::to_string(wrap) + "}");
				textures.push_back("{\"source\":" + std::to_string(images.size() - 1)
								   + ",\"sampler\":" + std::to_string(samplers.size() - 1) + "}");
				textureIndex[key] = int(textures.size()) - 1;
				return int(textures.size()) - 1;
			}

			/*! The frames of a part to export */
			static std::pair<int, int> exportFrames(const Model &model)
			{
				const int count = model.numFrames();
				if (count <= 0)
				{
					return {0, -1};
				}
				const int first = std::clamp(model.firstExportFrame, 0, count - 1);
				const int last = model.lastExportFrame < 0 ? count - 1 : std::clamp(model.lastExportFrame, first, count - 1);
				return {first, last};
			}

			/*! The sequences in the export frames, or one sequence of every export frame; frames in playing order */
			struct Sequence {
				std::string name;
				float fps;
				std::vector<int> frames;
			};

			std::vector<Sequence> sequences(const GltfPart &part, size_t index, int first, int last) const
			{
				const std::string prefix = index == 0 ? std::string() : part.name + ": ";
				std::vector<Sequence> out;
				for (const auto &animation : part.model->animations)
				{
					Sequence sequence{prefix + animation.name, animation.fps > 0 ? float(animation.fps) : DEFAULT_FPS, {}};
					for (int k = 0; k < animation.numFrames; k++)
					{
						const int frame
							= animation.reversed ? animation.startFrame + animation.numFrames - 1 - k : animation.startFrame + k;
						if (frame >= first && frame <= last)
						{
							sequence.frames.push_back(frame);
						}
					}
					if (!sequence.frames.empty())
					{
						out.push_back(std::move(sequence));
					}
				}
				if (out.empty() && last > first)
				{
					Sequence all{prefix + "frames", DEFAULT_FPS, {}};
					for (int frame = first; frame <= last; frame++)
					{
						all.frames.push_back(frame);
					}
					out.push_back(std::move(all));
				}
				return out;
			}

			std::vector<Channel> &animation(const std::string &name)
			{
				if (!animations.count(name))
				{
					animationOrder.push_back(name);
				}
				return animations[name];
			}

			static void addPoint(Channel &translation, Channel &rotation, float time, const Point &point)
			{
				translation.times.push_back(time);
				translation.values.insert(translation.values.end(), {point.location.x, point.location.y, point.location.z});
				glm::quat q = glm::normalize(point.rotation);
				// The shorter way from the previous key, so the interpolation does not spin
				if (rotation.values.size() >= 4)
				{
					const size_t last = rotation.values.size() - 4;
					const glm::quat previous(rotation.values[last + 3], rotation.values[last], rotation.values[last + 1],
											 rotation.values[last + 2]);
					if (glm::dot(previous, q) < 0.0f)
					{
						q = -q;
					}
				}
				rotation.times.push_back(time);
				rotation.values.insert(rotation.values.end(), {q.x, q.y, q.z, q.w});
			}

			PartNodes addPart(const GltfPart &part, size_t index, int parentNode)
			{
				Node node;
				node.name = part.name.empty() ? "part" + std::to_string(index) : part.name;
				const Point transform = point(part.model->transform);
				node.translation = transform.location;
				node.rotation = transform.rotation;
				node.scale = glm::vec3(part.model->scale);
				PartNodes placed;
				placed.node = addNode(node, parentNode);

				if (const auto *skeletal = dynamic_cast<const SkeletalModel *>(part.model))
				{
					addSkeletal(part, index, *skeletal, placed);
				}
				else if (const auto *morph = dynamic_cast<const MorphModel *>(part.model))
				{
					addMorph(part, index, *morph, placed);
				}
				else
				{
					warn("Part " + node.name + " is not a skeletal or morph model, only its node is written");
				}
				return placed;
			}

			/**
			 * glTF skins need one rest pose where every weight of a vertex puts it at the same place. Models without a
			 * bind pose (MDM with MDX) only have frames: the frame where the weights agree best is the closest to the
			 * pose the model was made in, and gives the smallest error in the other frames.
			 */
			static int restFrame(const SkeletalModel &model, int first, int last)
			{
				const int step = std::max(1, (last - first + 1) / 400);
				int best = first;
				double bestSpread = std::numeric_limits<double>::max();
				for (int frame = first; frame <= last; frame += step)
				{
					double spread = 0.0;
					for (const auto &mesh : model.meshes)
					{
						for (const auto &vertex : mesh.vertices)
						{
							if (vertex.weightCount < 2)
							{
								continue;
							}
							const glm::vec3 average = skeletal::posedVertex(model, mesh, vertex, frame);
							for (int w = 0; w < vertex.weightCount; w++)
							{
								const size_t at = size_t(vertex.startWeight + w);
								if (at >= mesh.weights.size())
								{
									continue;
								}
								const auto &weight = mesh.weights[at];
								if (weight.jointId < 0 || weight.jointId >= int(model.joints.size())
									|| size_t(frame) >= model.joints[size_t(weight.jointId)].frames.size())
								{
									continue;
								}
								const Point &joint = model.joints[size_t(weight.jointId)].frames[size_t(frame)];
								const glm::vec3 place = joint.location + joint.rotation * weight.pos;
								spread += double(weight.bias) * double(glm::distance(place, average));
							}
						}
					}
					if (spread < bestSpread)
					{
						bestSpread = spread;
						best = frame;
					}
				}
				return best;
			}

			static Point localPoint(const Point &parent, const Point &child)
			{
				const glm::quat inverse = glm::inverse(parent.rotation);
				return {inverse * (child.location - parent.location), glm::normalize(inverse * child.rotation)};
			}

			void addSkeletal(const GltfPart &part, size_t index, const SkeletalModel &model, PartNodes &placed)
			{
				const auto [first, last] = exportFrames(model);
				if (!model.hasBindPose && last < first)
				{
					warn("Skeletal model " + part.name + " has no bind pose and no frames, it is skipped");
					return;
				}
				const int rest = model.hasBindPose ? FRAME_BIND_POSE : restFrame(model, first, last);
				auto rawPose = [&](size_t joint, int frame) -> const Point & {
					const auto &bone = model.joints[joint];
					return frame == FRAME_BIND_POSE || bone.frames.empty() ? static_cast<const Point &>(bone) : bone.frames[size_t(frame)];
				};
				auto pose = [&](size_t joint, int frame) { return point(rawPose(joint, frame)); };
				auto local = [&](size_t joint, int frame) {
					const int parent = model.joints[joint].parentId;
					return parent >= 0 && parent < int(model.joints.size()) ? localPoint(pose(size_t(parent), frame), pose(joint, frame))
																			: pose(joint, frame);
				};

				// Joints: nodes with their local rest pose, children of their parent joints
				std::vector<int> jointNodes(model.joints.size(), -1);
				for (size_t j = 0; j < model.joints.size(); j++)
				{
					Node joint;
					joint.name = model.joints[j].name;
					const Point point = local(j, rest);
					joint.translation = point.location;
					joint.rotation = point.rotation;
					jointNodes[j] = addNode(joint, -1);
					placed.attach[tools::lowercase(joint.name)] = jointNodes[j];
				}
				for (size_t j = 0; j < model.joints.size(); j++)
				{
					const int parent = model.joints[j].parentId;
					const int parentNode = parent >= 0 && parent < int(model.joints.size()) ? jointNodes[size_t(parent)] : placed.node;
					nodes[size_t(parentNode)].children.push_back(jointNodes[j]);
				}

				// Tags on bones, they win over bones with the same name
				for (const auto &tag : model.tags)
				{
					if (tag.parentBone < 0 || tag.parentBone >= int(model.joints.size()))
					{
						continue;
					}
					Node tagNode;
					tagNode.name = tag.name;
					const Point offset = point(tag);
					tagNode.translation = offset.location;
					tagNode.rotation = offset.rotation;
					placed.attach[tools::lowercase(tag.name)] = addNode(tagNode, jointNodes[size_t(tag.parentBone)]);
				}

				// Skin: the inverse of each joint's rest pose
				std::vector<float> inverseBind;
				std::vector<std::string> jointList;
				for (size_t j = 0; j < model.joints.size(); j++)
				{
					const Point point = pose(j, rest);
					const glm::mat4 matrix = glm::inverse(glm::translate(point.location) * glm::toMat4(point.rotation));
					const float *values = glm::value_ptr(matrix);
					inverseBind.insert(inverseBind.end(), values, values + 16);
					jointList.push_back(std::to_string(jointNodes[j]));
				}
				int skin = -1;
				if (!model.joints.empty())
				{
					const int matrices = addFloats(inverseBind, 16, "MAT4", 0, false);
					skins.push_back("{\"name\":" + jsonString(part.name) + ",\"inverseBindMatrices\":" + std::to_string(matrices)
									+ ",\"joints\":" + joined(jointList) + "}");
					skin = int(skins.size()) - 1;
				}

				// Meshes in the rest pose with their 4 strongest weights
				size_t dropped = 0;
				for (const auto &mesh : model.meshes)
				{
					if (mesh.vertices.empty() || mesh.tris.empty())
					{
						continue;
					}
					std::vector<float> positions, normals, texCoords, weights;
					std::vector<uint16_t> joints;
					for (const auto &vertex : mesh.vertices)
					{
						const glm::vec3 place = position(skeletal::posedVertex(model, mesh, vertex, rest));
						positions.insert(positions.end(), {place.x, place.y, place.z});

						glm::vec3 normal = rest == FRAME_BIND_POSE ? vertex.bindNormal : glm::vec3(0.0f);
						std::map<int, float> influence;
						for (int w = 0; w < vertex.weightCount; w++)
						{
							const size_t at = size_t(vertex.startWeight + w);
							if (at >= mesh.weights.size())
							{
								continue;
							}
							const auto &weight = mesh.weights[at];
							influence[weight.jointId] += weight.bias;
							if (rest != FRAME_BIND_POSE && weight.jointId >= 0 && weight.jointId < int(model.joints.size()))
							{
								normal += (rawPose(size_t(weight.jointId), rest).rotation * vertex.normal) * weight.bias;
							}
						}
						const float length = glm::length(normal);
						normal = direction(length > 0.0f ? normal / length : glm::vec3(0, 0, 1));
						normals.insert(normals.end(), {normal.x, normal.y, normal.z});
						texCoords.insert(texCoords.end(), {vertex.tex.x, vertex.tex.y});

						std::vector<std::pair<float, int>> sorted;
						for (const auto &[joint, bias] : influence)
						{
							if (joint >= 0 && joint < int(model.joints.size()) && bias > 0.0f)
							{
								sorted.push_back({bias, joint});
							}
						}
						std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
						dropped += sorted.size() > 4 ? 1 : 0;
						sorted.resize(std::min<size_t>(sorted.size(), 4));
						float total = 0.0f;
						for (const auto &entry : sorted)
						{
							total += entry.first;
						}
						for (size_t k = 0; k < 4; k++)
						{
							const bool used = k < sorted.size() && total > 0.0f;
							joints.push_back(used ? uint16_t(sorted[k].second) : 0);
							weights.push_back(used ? sorted[k].first / total : (k == 0 && sorted.empty() ? 1.0f : 0.0f));
						}
					}

					std::string attributes
						= "\"POSITION\":" + std::to_string(addFloats(positions, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, true))
						  + ",\"NORMAL\":" + std::to_string(addFloats(normals, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, false))
						  + ",\"TEXCOORD_0\":" + std::to_string(addFloats(texCoords, 2, "VEC2", GL_ARRAY_BUFFER_TARGET, false));
					if (skin >= 0)
					{
						attributes += ",\"JOINTS_0\":" + std::to_string(addJoints(joints))
									  + ",\"WEIGHTS_0\":" + std::to_string(addFloats(weights, 4, "VEC4", GL_ARRAY_BUFFER_TARGET, false));
					}
					meshes.push_back("{\"name\":" + jsonString(mesh.name) + ",\"primitives\":[{\"attributes\":{" + attributes
									 + "},\"indices\":" + std::to_string(addIndices(mesh.tris))
									 + ",\"material\":" + std::to_string(material(part, mesh.name, mesh.materialName)) + "}]}");

					Node meshNode;
					meshNode.name = mesh.name;
					meshNode.mesh = int(meshes.size()) - 1;
					meshNode.skin = skin;
					addNode(meshNode, placed.node);
				}
				if (dropped > 0)
				{
					warn(std::to_string(dropped) + " vertices of " + part.name + " have more than 4 weights, the 4 strongest are kept");
				}

				// Animations: the joints' local translation and rotation in every frame of each sequence
				if (last < first)
				{
					return;
				}
				for (const auto &sequence : sequences(part, index, first, last))
				{
					auto &channels = animation(sequence.name);
					for (size_t j = 0; j < model.joints.size(); j++)
					{
						Channel translation{jointNodes[j], "translation", {}, {}};
						Channel rotation{jointNodes[j], "rotation", {}, {}};
						for (size_t k = 0; k < sequence.frames.size(); k++)
						{
							addPoint(translation, rotation, float(k) / sequence.fps, local(j, sequence.frames[k]));
						}
						channels.push_back(std::move(translation));
						channels.push_back(std::move(rotation));
					}
				}
			}

			void addMorph(const GltfPart &part, size_t index, const MorphModel &model, PartNodes &placed)
			{
				const auto [first, last] = exportFrames(model);
				if (last < first)
				{
					warn("Model " + part.name + " has no frames, it is skipped");
					return;
				}
				const bool morph = options.morphTargets && last > first;
				const int targetCount = morph ? last - first + 1 : 0;

				std::vector<int> meshNodes;
				for (const auto &mesh : model.meshes)
				{
					if (mesh.tris.empty() || int(mesh.frames.size()) <= last)
					{
						continue;
					}
					const auto &base = mesh.frames[size_t(first)];
					std::vector<float> positions, normals, texCoords;
					for (size_t v = 0; v < base.size(); v++)
					{
						const glm::vec3 place = position(base[v].pos);
						const glm::vec3 normal = direction(base[v].normal);
						positions.insert(positions.end(), {place.x, place.y, place.z});
						normals.insert(normals.end(), {normal.x, normal.y, normal.z});
						const glm::vec2 tex = v < mesh.texCoords.size() ? mesh.texCoords[v] : glm::vec2(0.0f);
						texCoords.insert(texCoords.end(), {tex.x, tex.y});
					}

					std::string primitive
						= "{\"attributes\":{\"POSITION\":" + std::to_string(addFloats(positions, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, true))
						  + ",\"NORMAL\":" + std::to_string(addFloats(normals, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, false))
						  + ",\"TEXCOORD_0\":" + std::to_string(addFloats(texCoords, 2, "VEC2", GL_ARRAY_BUFFER_TARGET, false))
						  + "},\"indices\":" + std::to_string(addIndices(mesh.tris))
						  + ",\"material\":" + std::to_string(material(part, mesh.name, mesh.materialName));

					// One target per export frame: the difference to the first export frame
					if (morph)
					{
						std::vector<std::string> targets;
						for (int frame = first; frame <= last; frame++)
						{
							const auto &vertices = mesh.frames[size_t(frame)];
							std::vector<float> deltaPositions, deltaNormals;
							for (size_t v = 0; v < base.size(); v++)
							{
								const glm::vec3 dp = position(vertices[v].pos - base[v].pos);
								const glm::vec3 dn = direction(vertices[v].normal - base[v].normal);
								deltaPositions.insert(deltaPositions.end(), {dp.x, dp.y, dp.z});
								deltaNormals.insert(deltaNormals.end(), {dn.x, dn.y, dn.z});
							}
							targets.push_back(
								"{\"POSITION\":" + std::to_string(addFloats(deltaPositions, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, true))
								+ ",\"NORMAL\":" + std::to_string(addFloats(deltaNormals, 3, "VEC3", GL_ARRAY_BUFFER_TARGET, false)) + "}");
						}
						primitive += ",\"targets\":" + joined(targets);
					}
					meshes.push_back("{\"name\":" + jsonString(mesh.name) + ",\"primitives\":[" + primitive + "}]}");

					Node meshNode;
					meshNode.name = mesh.name;
					meshNode.mesh = int(meshes.size()) - 1;
					if (morph)
					{
						meshNode.weights.assign(size_t(targetCount), 0.0f);
						meshNode.weights[0] = 1.0f;
					}
					meshNodes.push_back(addNode(meshNode, placed.node));
				}

				// Tags: nodes at the first export frame, animated with the frames
				std::vector<int> tagNodes;
				for (const auto &tag : model.tags)
				{
					Node tagNode;
					tagNode.name = tag.name;
					if (size_t(first) < tag.frames.size())
					{
						const Point place = point(tag.frames[size_t(first)]);
						tagNode.translation = place.location;
						tagNode.rotation = place.rotation;
					}
					tagNodes.push_back(addNode(tagNode, placed.node));
					placed.attach[tools::lowercase(tag.name)] = tagNodes.back();
				}

				for (const auto &sequence : sequences(part, index, first, last))
				{
					auto &channels = animation(sequence.name);
					if (morph)
					{
						// The target of each frame at full weight, the others at zero
						std::vector<float> times, values;
						for (size_t k = 0; k < sequence.frames.size(); k++)
						{
							times.push_back(float(k) / sequence.fps);
							for (int target = 0; target < targetCount; target++)
							{
								values.push_back(target == sequence.frames[k] - first ? 1.0f : 0.0f);
							}
						}
						for (int meshNode : meshNodes)
						{
							channels.push_back({meshNode, "weights", times, values});
						}
					}
					for (size_t t = 0; t < model.tags.size(); t++)
					{
						Channel translation{tagNodes[t], "translation", {}, {}};
						Channel rotation{tagNodes[t], "rotation", {}, {}};
						for (size_t k = 0; k < sequence.frames.size(); k++)
						{
							const size_t frame = size_t(sequence.frames[k]);
							if (frame < model.tags[t].frames.size())
							{
								addPoint(translation, rotation, float(k) / sequence.fps, point(model.tags[t].frames[frame]));
							}
						}
						if (!translation.times.empty())
						{
							channels.push_back(std::move(translation));
							channels.push_back(std::move(rotation));
						}
					}
				}
			}

			std::vector<byte> finish()
			{
				// Animations: one sampler per channel
				std::vector<std::string> animationJson;
				for (const auto &name : animationOrder)
				{
					std::vector<std::string> channelJson, samplerJson;
					for (const auto &channel : animations[name])
					{
						const int input = addFloats(channel.times, 1, "SCALAR", 0, true);
						const int components = channel.path == "rotation" ? 4 : channel.path == "translation" ? 3 : 1;
						const char *type = components == 4 ? "VEC4" : components == 3 ? "VEC3" : "SCALAR";
						const int output = addFloats(channel.values, components, type, 0, false);
						samplerJson.push_back("{\"input\":" + std::to_string(input) + ",\"output\":" + std::to_string(output)
											  + ",\"interpolation\":\"LINEAR\"}");
						channelJson.push_back("{\"sampler\":" + std::to_string(samplerJson.size() - 1) + ",\"target\":{\"node\":"
											  + std::to_string(channel.node) + ",\"path\":" + jsonString(channel.path) + "}}");
					}
					if (!channelJson.empty())
					{
						animationJson.push_back("{\"name\":" + jsonString(name) + ",\"channels\":" + joined(channelJson)
												+ ",\"samplers\":" + joined(samplerJson) + "}");
					}
				}

				std::vector<std::string> nodeJson;
				for (const auto &node : nodes)
				{
					std::string json = "{\"name\":" + jsonString(node.name);
					if (node.translation != glm::vec3(0.0f))
					{
						json += ",\"translation\":" + list(std::vector<float>{node.translation.x, node.translation.y, node.translation.z});
					}
					const glm::quat q = glm::normalize(node.rotation);
					if (q != glm::quat(1, 0, 0, 0))
					{
						json += ",\"rotation\":" + list(std::vector<float>{q.x, q.y, q.z, q.w});
					}
					if (node.scale != glm::vec3(1.0f))
					{
						json += ",\"scale\":" + list(std::vector<float>{node.scale.x, node.scale.y, node.scale.z});
					}
					if (node.mesh >= 0)
					{
						json += ",\"mesh\":" + std::to_string(node.mesh);
					}
					if (node.skin >= 0)
					{
						json += ",\"skin\":" + std::to_string(node.skin);
					}
					if (!node.weights.empty())
					{
						json += ",\"weights\":" + list(node.weights);
					}
					if (!node.children.empty())
					{
						json += ",\"children\":" + list(node.children);
					}
					nodeJson.push_back(json + "}");
				}

				while (buffer.size() % 4)
				{
					buffer.push_back(0);
				}

				std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"ModelTool\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}]";
				json += ",\"nodes\":" + joined(nodeJson);
				auto add = [&](const char *key, const std::vector<std::string> &items) {
					if (!items.empty())
					{
						json += std::string(",\"") + key + "\":" + joined(items);
					}
				};
				add("meshes", meshes);
				add("materials", materials);
				add("textures", textures);
				add("images", images);
				add("samplers", samplers);
				add("skins", skins);
				add("animations", animationJson);
				add("accessors", accessors);
				add("bufferViews", bufferViews);
				if (!buffer.empty())
				{
					std::string bufferJson = "{\"byteLength\":" + std::to_string(buffer.size());
					if (!options.binary)
					{
						bufferJson += ",\"uri\":" + jsonString("data:application/octet-stream;base64," + base64(buffer));
					}
					json += ",\"buffers\":[" + bufferJson + "}]";
				}
				json += "}";

				if (!options.binary)
				{
					return {json.begin(), json.end()};
				}

				// GLB: header, JSON chunk (padded with spaces), binary chunk
				while (json.size() % 4)
				{
					json += ' ';
				}
				std::vector<byte> out;
				auto put = [&](uint32_t value) {
					for (int b = 0; b < 4; b++)
					{
						out.push_back(byte((value >> (8 * b)) & 0xff));
					}
				};
				const uint32_t total = 12 + 8 + uint32_t(json.size()) + (buffer.empty() ? 0 : 8 + uint32_t(buffer.size()));
				put(0x46546C67); // glTF
				put(2);
				put(total);
				put(uint32_t(json.size()));
				put(0x4E4F534A); // JSON
				out.insert(out.end(), json.begin(), json.end());
				if (!buffer.empty())
				{
					put(uint32_t(buffer.size()));
					put(0x004E4942); // BIN
					out.insert(out.end(), buffer.begin(), buffer.end());
				}
				return out;
			}
		};
	}

	std::vector<byte> encodeGLTF(const std::vector<GltfPart> &parts, const GltfOptions &options)
	{
		return Writer(parts, options).write();
	}

	void writeGLTF(const std::vector<GltfPart> &parts, const std::string &path, GltfOptions options)
	{
		options.binary = tools::lowercase(std::filesystem::path(path).extension().string()) != ".gltf";
		const auto data = encodeGLTF(parts, options);
		std::ofstream out(path, std::ios::binary);
		if (!out)
		{
			throw mt_ex("Can not write " + path);
		}
		out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	}

	void writeGLTF(const Model &model, const std::string &path)
	{
		GltfPart part;
		part.model = &model;
		part.name = std::filesystem::path(model.name.empty() ? path : model.name).stem().string();
		writeGLTF({part}, path, GltfOptions{});
	}
}
