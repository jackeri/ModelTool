#include "model_ufbx.h"
#include "scene_import.h"

#include <ufbx.h>

#include <filesystem>
#include <map>
#include <tuple>

namespace mt::model {

	namespace {
		/*! Frees the scene when going out of scope */
		struct SceneRef {
			ufbx_scene *scene = nullptr;

			~SceneRef()
			{
				ufbx_free_scene(scene);
			}
		};

		glm::vec3 toVec3(const ufbx_vec3 &vec)
		{
			return {float(vec.x), float(vec.y), float(vec.z)};
		}

		glm::mat4 toMat4(const ufbx_matrix &m)
		{
			glm::mat4 out{1.0f};
			for (int c = 0; c < 4; c++)
			{
				out[c] = glm::vec4(toVec3(m.cols[c]), c == 3 ? 1.0f : 0.0f);
			}
			return out;
		}

		std::string toString(const ufbx_string &value)
		{
			return {value.data, value.length};
		}

		std::string materialName(const ufbx_material *material)
		{
			if (!material)
			{
				return "";
			}

			const ufbx_texture *texture
				= material->fbx.diffuse_color.texture ? material->fbx.diffuse_color.texture : material->pbr.base_color.texture;
			if (texture)
			{
				std::string path = toString(texture->relative_filename.length ? texture->relative_filename : texture->filename);
				std::replace(path.begin(), path.end(), '\\', '/');
				if (!path.empty())
				{
					return path;
				}
			}

			return toString(material->name);
		}

		/*! Converts a loaded ufbx scene into the intermediate scene */
		class Converter {
		  public:
			Converter(ufbx_scene *scene, import::Scene &target) : scene(scene), target(target) {}

			void convert()
			{
				target.skinned = std::any_of(scene->meshes.data, scene->meshes.data + scene->meshes.count,
											 [](const ufbx_mesh *mesh) { return mesh->skin_deformers.count > 0; });

				collectNodes();
				sampleAnimations();
				chooseReferencePose();

				for (size_t i = 0; i < scene->nodes.count; i++)
				{
					const ufbx_node *node = scene->nodes.data[i];
					if (node->mesh)
					{
						addMesh(node);
					}
				}
			}

		  private:
			/*! Node is used as a joint (bone, the node of a rigid mesh in a skinned scene, or a tag) */
			bool usedAsJoint(const ufbx_node *node)
			{
				if (import::isTagName(toString(node->name)))
				{
					return true;
				}

				if (!target.skinned)
				{
					return false;
				}

				if (node->bone)
				{
					return true;
				}

				for (size_t i = 0; i < scene->skin_clusters.count; i++)
				{
					if (scene->skin_clusters.data[i]->bone_node == node)
					{
						return true;
					}
				}

				return node->mesh && node->mesh->skin_deformers.count == 0;
			}

			void collectNodes()
			{
				// Joints and their parents up to the root
				std::vector<bool> used(scene->nodes.count, false);
				for (size_t i = 0; i < scene->nodes.count; i++)
				{
					const ufbx_node *node = scene->nodes.data[i];
					if (!usedAsJoint(node))
					{
						continue;
					}

					for (const ufbx_node *current = node; current && !current->is_root; current = current->parent)
					{
						used[current->typed_id] = true;
					}
				}

				// Depth first from the root, so the parents come before the children
				std::vector<const ufbx_node *> stack{scene->root_node};
				while (!stack.empty())
				{
					const ufbx_node *node = stack.back();
					stack.pop_back();

					if (!node->is_root && used[node->typed_id])
					{
						import::Node joint;
						joint.name = toString(node->name);
						joint.bind = toMat4(node->node_to_world);

						for (const ufbx_node *parent = node->parent; parent && !parent->is_root; parent = parent->parent)
						{
							auto found = nodeIndex.find(parent->typed_id);
							if (found != nodeIndex.end())
							{
								joint.parent = found->second;
								break;
							}
						}

						nodeIndex[node->typed_id] = int(target.nodes.size());
						if (import::isTagName(joint.name))
						{
							target.tags.push_back(int(target.nodes.size()));
						}
						target.nodes.push_back(joint);
					}

					for (size_t c = node->children.count; c > 0; c--)
					{
						stack.push_back(node->children.data[c - 1]);
					}
				}

				// The bind pose of the bones comes from the skin clusters (see chooseReferencePose)
				for (size_t i = 0; i < scene->skin_clusters.count; i++)
				{
					const ufbx_skin_cluster *cluster = scene->skin_clusters.data[i];
					if (cluster->bone_node)
					{
						auto found = nodeIndex.find(cluster->bone_node->typed_id);
						if (found != nodeIndex.end())
						{
							target.nodes[found->second].bind = toMat4(cluster->bind_to_world);
						}
					}
				}
			}

			void sampleAnimations()
			{
				const double fps = scene->settings.frames_per_second > 0 ? scene->settings.frames_per_second : import::DEFAULT_FPS;

				for (size_t s = 0; s < scene->anim_stacks.count; s++)
				{
					const ufbx_anim_stack *stack = scene->anim_stacks.data[s];
					const double length = std::max(stack->time_end - stack->time_begin, 0.0);
					const int numFrames = int(std::lround(length * fps)) + 1;

					import::Animation animation;
					animation.name = toString(stack->name);
					animation.startFrame = target.numFrames;
					animation.numFrames = numFrames;
					animation.fps = int(std::lround(fps));

					for (int frame = 0; frame < numFrames; frame++)
					{
						double time = stack->time_begin + double(frame) / fps;
						ufbx_evaluate_opts opts{};
						ufbx_error error;
						SceneRef evaluated{ufbx_evaluate_scene(scene, stack->anim, time, &opts, &error)};
						if (!evaluated.scene)
						{
							throw mt_ex("Failed to evaluate the animation " + animation.name + ": " + toString(error.description));
						}

						for (const auto &[typedId, index] : nodeIndex)
						{
							target.nodes[index].frames.push_back(toMat4(evaluated.scene->nodes.data[typedId]->node_to_world));
						}

						if (!target.skinned)
						{
							frameMatrices.emplace_back();
							for (size_t n = 0; n < evaluated.scene->nodes.count; n++)
							{
								const ufbx_node *node = evaluated.scene->nodes.data[n];
								frameMatrices.back().emplace_back(toMat4(node->geometry_to_world),
																  toMat4(ufbx_get_compatible_matrix_for_normals(node)));
							}
						}
					}

					target.numFrames += numFrames;
					target.animations.push_back(animation);
				}
			}

			/**
			 * Our bones can not scale. If a bone has a different scale in the bind pose than in the animation (motion
			 * capture files rescale the skeleton), the first animation frame is used as the bind pose instead, so the
			 * weights match the animation.
			 */
			void chooseReferencePose()
			{
				if (!target.skinned || target.numFrames == 0)
				{
					return;
				}

				auto scaleOf = [](const glm::mat4 &matrix) {
					return glm::vec3{glm::length(glm::vec3(matrix[0])), glm::length(glm::vec3(matrix[1])),
									 glm::length(glm::vec3(matrix[2]))};
				};

				for (const auto &node : target.nodes)
				{
					glm::vec3 bind = scaleOf(node.bind);
					glm::vec3 frame = scaleOf(node.frames.front());
					if (glm::any(glm::greaterThan(glm::abs(bind - frame), 0.01f * glm::max(bind, frame))))
					{
						useFirstFrame = true;
						break;
					}
				}

				if (useFirstFrame)
				{
					for (auto &node : target.nodes)
					{
						node.bind = node.frames.front();
					}
				}
			}

			/**
			 * World transform of a bone in the reference pose
			 */
			glm::mat4 referenceBone(const ufbx_skin_cluster *cluster) const
			{
				if (useFirstFrame)
				{
					return target.nodes[nodeIndex.at(cluster->bone_node->typed_id)].frames.front();
				}
				return toMat4(cluster->bind_to_world);
			}

			void addMesh(const ufbx_node *node)
			{
				const ufbx_mesh *mesh = node->mesh;
				const ufbx_skin_deformer *skin = mesh->skin_deformers.count > 0 ? mesh->skin_deformers.data[0] : nullptr;
				const glm::mat4 geometryToWorld = toMat4(node->geometry_to_world);
				const glm::mat3 normalMatrix = glm::mat3(toMat4(ufbx_get_compatible_matrix_for_normals(node)));

				std::vector<uint32_t> triangles(mesh->max_face_triangles * 3);

				for (size_t p = 0; p < mesh->material_parts.count; p++)
				{
					const ufbx_mesh_part &part = mesh->material_parts.data[p];
					if (part.num_triangles == 0)
					{
						continue;
					}

					import::Mesh out;
					out.name = toString(node->name);
					const ufbx_material *material = part.index < node->materials.count	 ? node->materials.data[part.index]
													: part.index < mesh->materials.count ? mesh->materials.data[part.index]
																						 : nullptr;
					out.material = materialName(material);
					if (mesh->material_parts.count > 1 && material)
					{
						out.name += "_" + toString(material->name);
					}

					std::map<std::tuple<uint32_t, float, float, float, float, float>, int> corners;
					std::vector<uint32_t> logical; // file vertex of each output vertex

					for (uint32_t faceIndex :
						 std::vector<uint32_t>(part.face_indices.data, part.face_indices.data + part.face_indices.count))
					{
						const ufbx_face face = mesh->faces.data[faceIndex];
						uint32_t numTris = ufbx_triangulate_face(triangles.data(), triangles.size(), mesh, face);

						for (uint32_t t = 0; t < numTris; t++)
						{
							glm::ivec3 tri;
							for (int c = 0; c < 3; c++)
							{
								uint32_t index = triangles[t * 3 + c];
								glm::vec3 normal
									= mesh->vertex_normal.exists ? toVec3(ufbx_get_vertex_vec3(&mesh->vertex_normal, index)) : glm::vec3{};
								glm::vec2 uv{};
								if (mesh->vertex_uv.exists)
								{
									ufbx_vec2 value = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
									// FBX and OBJ have the texture origin at the bottom, the game at the top
									uv = {float(value.x), 1.0f - float(value.y)};
								}

								uint32_t vertexIndex = mesh->vertex_indices.data[index];
								auto key = std::make_tuple(vertexIndex, uv.x, uv.y, normal.x, normal.y, normal.z);
								auto found = corners.find(key);
								if (found != corners.end())
								{
									tri[c] = found->second;
									continue;
								}

								import::Vertex vert;
								glm::vec3 pos = toVec3(mesh->vertices.data[vertexIndex]);
								vert.tex = uv;

								if (skin)
								{
									addSkinWeights(skin, vertexIndex, pos, normal, vert);
								}
								else
								{
									vert.pos = glm::vec3(geometryToWorld * glm::vec4(pos, 1.0f));
									vert.normal = normalMatrix * normal;
									if (target.skinned)
									{
										vert.influences.push_back({nodeIndex.at(node->typed_id), 1.0f});
									}
								}

								tri[c] = int(out.vertices.size());
								corners.emplace(key, tri[c]);
								out.vertices.push_back(vert);
								logical.push_back(vertexIndex);
							}

							// ufbx triangles are counter-clockwise, idTech is clockwise
							out.tris.emplace_back(tri[0], tri[2], tri[1]);
						}
					}

					if (!target.skinned && !frameMatrices.empty())
					{
						addStaticFrames(node, mesh, logical, out);
					}

					target.meshes.push_back(std::move(out));
				}
			}

			void addSkinWeights(const ufbx_skin_deformer *skin, uint32_t vertexIndex, const glm::vec3 &pos, const glm::vec3 &normal,
								import::Vertex &vert)
			{
				if (vertexIndex >= skin->vertices.count)
				{
					throw mt_ex("Skinned vertex without weights: " + std::to_string(vertexIndex));
				}

				const ufbx_skin_vertex &skinVertex = skin->vertices.data[vertexIndex];
				float total = 0;
				glm::vec3 bindPos{0};
				glm::vec3 bindNormal{0};

				for (uint32_t w = 0; w < skinVertex.num_weights; w++)
				{
					const ufbx_skin_weight &weight = skin->weights.data[skinVertex.weight_begin + w];
					const ufbx_skin_cluster *cluster = skin->clusters.data[weight.cluster_index];
					if (!cluster->bone_node || weight.weight <= 0)
					{
						continue;
					}

					// Vertex in the reference pose: bone transform * position in the bone space
					glm::mat4 bind = referenceBone(cluster) * toMat4(cluster->geometry_to_bone);
					bindPos += glm::vec3(bind * glm::vec4(pos, 1.0f)) * float(weight.weight);
					bindNormal += glm::transpose(glm::inverse(glm::mat3(bind))) * normal * float(weight.weight);
					total += float(weight.weight);

					vert.influences.push_back({nodeIndex.at(cluster->bone_node->typed_id), float(weight.weight)});
				}

				if (total <= 0)
				{
					throw mt_ex("Skinned vertex without weights: " + std::to_string(vertexIndex));
				}

				vert.pos = bindPos / total;
				vert.normal = bindNormal;
			}

			void addStaticFrames(const ufbx_node *node, const ufbx_mesh *mesh, const std::vector<uint32_t> &logical, import::Mesh &out)
			{
				const auto &normals = out.vertices;
				for (const auto &matrices : frameMatrices)
				{
					const auto &[geometry, normalMatrix] = matrices.at(node->typed_id);
					std::vector<glm::vec3> positions, frameNormals;

					for (size_t v = 0; v < logical.size(); v++)
					{
						positions.emplace_back(geometry * glm::vec4(toVec3(mesh->vertices.data[logical[v]]), 1.0f));
					}

					// Normals in the file space: undo the bind transform of the vertex normal
					const glm::mat3 bindNormal = glm::mat3(toMat4(ufbx_get_compatible_matrix_for_normals(node)));
					const glm::mat3 toFrame = glm::mat3(normalMatrix) * glm::inverse(bindNormal);
					for (const auto &vert : normals)
					{
						frameNormals.push_back(toFrame * vert.normal);
					}

					out.framePositions.push_back(std::move(positions));
					out.frameNormals.push_back(std::move(frameNormals));
				}
			}

			ufbx_scene *scene;
			import::Scene &target;
			std::map<uint32_t, int> nodeIndex{}; ///< ufbx node typed id -> scene node
			bool useFirstFrame = false;			 ///< the first frame is the bind pose
			std::vector<std::vector<std::pair<glm::mat4, glm::mat4>>>
				frameMatrices{}; ///< static scenes: geometry and normal matrices per frame
		};

		Model *load(const Ref<mt::io::MTFile> &file, ufbx_file_format format)
		{
			if (!file || !file->valid())
			{
				throw mt_ex("File not found");
			}

			ufbx_load_opts opts{};
			opts.file_format = format;
			opts.load_external_files = false;
			opts.generate_missing_normals = true;
			// Only the axes are converted. The units FBX files declare are not reliable for game models (the sample
			// files use game units with "centimeters"), so the coordinates are kept as they are.
			opts.target_axes = ufbx_axes_right_handed_z_up;

			const size_t length = file->len ? std::min(file->len, file->data->size()) : file->data->size();
			ufbx_error error;
			SceneRef scene{ufbx_load_memory(file->data->data(), length, &opts, &error)};
			if (!scene.scene)
			{
				throw mt_ex("Failed to load " + file->name + ": " + toString(error.description));
			}

			import::Scene out;
			out.name = std::filesystem::path(file->name).stem().string();
			Converter(scene.scene, out).convert();

			if (out.meshes.empty())
			{
				throw mt_ex("The file has no meshes: " + file->name);
			}

			return import::toModel(std::move(out));
		}
	}

	Model *loadFBX(const Ref<mt::io::MTFile> &file)
	{
		return load(file, UFBX_FILE_FORMAT_FBX);
	}

	Model *loadOBJ(const Ref<mt::io::MTFile> &file)
	{
		return load(file, UFBX_FILE_FORMAT_OBJ);
	}
}
