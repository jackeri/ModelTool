#include "scene_import.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "morph_model.h"
#include "normals.h"
#include "et_armature.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <limits>

namespace mt::model::import {

	bool isTagName(const std::string &name)
	{
		return name.size() >= 4 && std::equal(name.begin(), name.begin() + 4, "tag_", [](char a, char b) { return std::tolower(a) == b; });
	}

	namespace {
		/*! Applies the scene's axis and unit conversion */
		struct Conversion {
			glm::mat3 rotation;
			float scale;

			[[nodiscard]] glm::vec3 position(const glm::vec3 &pos) const
			{
				return rotation * pos * scale;
			}

			[[nodiscard]] glm::vec3 direction(const glm::vec3 &dir) const
			{
				glm::vec3 out = rotation * dir;
				float length = glm::length(out);
				return length > 0.0f ? out / length : out;
			}

			/**
			 * Rigid part of a world transform, converted. Scaling in the transform is dropped (bones can not scale).
			 */
			[[nodiscard]] Point point(const glm::mat4 &world) const
			{
				glm::vec3 scaleOut, translation, skew;
				glm::vec4 perspective;
				glm::quat orientation;
				if (!glm::decompose(world, scaleOut, orientation, translation, skew, perspective))
				{
					throw mt_ex("Invalid node transform");
				}

				// Mirrored transforms decompose with a negative scale, keep the rotation proper
				orientation = glm::normalize(orientation);

				glm::quat axis = glm::quat_cast(rotation);
				return {position(translation), glm::normalize(axis * orientation * glm::inverse(axis))};
			}
		};

		void checkFrames(const Scene &scene)
		{
			for (const auto &node : scene.nodes)
			{
				if (!node.frames.empty() && int(node.frames.size()) != scene.numFrames)
				{
					throw mt_ex("Node " + node.name + " does not have every frame");
				}
			}

			for (size_t i = 0; i < scene.nodes.size(); i++)
			{
				if (scene.nodes[i].parent >= int(i))
				{
					throw mt_ex("Node " + scene.nodes[i].name + " comes before its parent");
				}
			}
		}

		Model *toSkeletal(Scene &scene, const Conversion &convert)
		{
			auto model = std::make_unique<skeletal::SkeletalModel>();
			model->name = scene.name;
			model->hasBindPose = true;

			for (const auto &node : scene.nodes)
			{
				skeletal::Joint joint;
				joint.name = node.name;
				joint.parentId = node.parent;
				static_cast<Point &>(joint) = convert.point(node.bind);

				for (int frame = 0; frame < scene.numFrames; frame++)
				{
					joint.frames.push_back(node.frames.empty() ? convert.point(node.bind) : convert.point(node.frames[frame]));
				}

				model->joints.push_back(joint);
			}

			for (auto &source : scene.meshes)
			{
				skeletal::Mesh mesh;
				mesh.name = source.name;
				mesh.materialName = source.material;
				mesh.tris = source.tris;

				std::vector<glm::vec3> normals;
				for (const auto &vert : source.vertices)
				{
					skeletal::SkeletalVertex out;
					out.pos = convert.position(vert.pos);
					out.tex = vert.tex;
					out.startWeight = int(mesh.weights.size());

					float total = 0;
					for (const auto &influence : vert.influences)
					{
						if (influence.weight > 0.0f)
						{
							total += influence.weight;
						}
					}

					for (const auto &influence : vert.influences)
					{
						if (influence.weight <= 0.0f)
						{
							continue;
						}

						if (influence.node < 0 || influence.node >= int(model->joints.size()))
						{
							throw mt_ex("Vertex has a weight for an invalid bone: " + std::to_string(influence.node));
						}

						// Our weights are positions in the (rigid) bone space
						const Point &bone = model->joints[influence.node];
						glm::vec3 local = glm::inverse(bone.rotation) * (out.pos - bone.location);
						mesh.weights.push_back({influence.node, influence.weight / total, local});
						out.weightCount++;
					}

					if (out.weightCount == 0)
					{
						throw mt_ex("Mesh " + source.name + " has a vertex that is not bound to a bone");
					}

					mesh.vertices.push_back(out);
					normals.push_back(convert.direction(vert.normal));
				}

				for (const auto &tri : mesh.tris)
				{
					for (int i = 0; i < 3; i++)
					{
						if (tri[i] < 0 || tri[i] >= int(mesh.vertices.size()))
						{
							throw mt_ex("Mesh " + source.name + " has a triangle pointing to an invalid vertex");
						}
					}
				}

				setNormals(*model, mesh, normals, FRAME_BIND_POSE);
				model->meshes.push_back(std::move(mesh));
			}

			skeletal::optimizeVertices(*model);
			for (auto &mesh : model->meshes)
			{
				mesh.minLod = int(mesh.vertices.size());
			}

			et::markTagBones(*model);
			et::applyDefaultJointInformation(*model);

			if (scene.numFrames > 0)
			{
				skeletal::calculateBounds(*model);
			}

			return model.release();
		}

		Model *toMorph(Scene &scene, const Conversion &convert)
		{
			auto model = std::make_unique<morph::MorphModel>();
			model->name = scene.name;

			const int numFrames = std::max(scene.numFrames, 1);
			model->frames.resize(size_t(numFrames));

			for (auto &source : scene.meshes)
			{
				morph::Mesh mesh;
				mesh.name = source.name;
				mesh.materialName = source.material;
				mesh.tris = source.tris;

				for (const auto &vert : source.vertices)
				{
					mesh.texCoords.push_back(vert.tex);
				}

				for (int frame = 0; frame < numFrames; frame++)
				{
					const bool animated = frame < int(source.framePositions.size());
					std::vector<morph::FrameVertex> vertices(source.vertices.size());

					for (size_t v = 0; v < source.vertices.size(); v++)
					{
						glm::vec3 pos = animated ? source.framePositions[frame].at(v) : source.vertices[v].pos;
						glm::vec3 normal = animated ? source.frameNormals[frame].at(v) : source.vertices[v].normal;
						vertices[v] = {convert.position(pos), convert.direction(normal)};
					}

					mesh.frames.push_back(std::move(vertices));
				}

				model->meshes.push_back(std::move(mesh));
			}

			for (int tag : scene.tags)
			{
				const Node &node = scene.nodes.at(size_t(tag));
				morph::Tag out;
				out.name = node.name;
				for (int frame = 0; frame < numFrames; frame++)
				{
					out.frames.push_back(convert.point(node.frames.empty() ? node.bind : node.frames[frame]));
				}
				model->tags.push_back(out);
			}

			// Frame bounds from the vertices
			for (int frame = 0; frame < numFrames; frame++)
			{
				glm::vec3 min{std::numeric_limits<float>::max()}, max{std::numeric_limits<float>::lowest()};
				bool any = false;
				for (const auto &mesh : model->meshes)
				{
					for (const auto &vert : mesh.frames[frame])
					{
						min = glm::min(min, vert.pos);
						max = glm::max(max, vert.pos);
						any = true;
					}
				}

				auto &info = model->frames[frame];
				if (any)
				{
					info.bounds = {min, max};
					info.localOrigin = (min + max) * 0.5f;
					info.radius = glm::distance(info.localOrigin, max);
				}
			}

			model->validate();
			return model.release();
		}
	}

	/**
	 * Static scenes whose animations do not move anything (only other properties were animated) get a single frame
	 */
	static void dropStillFrames(Scene &scene)
	{
		if (scene.skinned || scene.numFrames == 0)
		{
			return;
		}

		for (const auto &mesh : scene.meshes)
		{
			for (const auto &frame : mesh.framePositions)
			{
				for (size_t v = 0; v < frame.size(); v++)
				{
					if (glm::distance(frame[v], mesh.vertices[v].pos) > 1e-5f)
					{
						return;
					}
				}
			}
		}

		for (int tag : scene.tags)
		{
			for (const auto &frame : scene.nodes[tag].frames)
			{
				if (frame != scene.nodes[tag].bind)
				{
					return;
				}
			}
		}

		for (auto &mesh : scene.meshes)
		{
			mesh.framePositions.clear();
			mesh.frameNormals.clear();
		}
		for (auto &node : scene.nodes)
		{
			node.frames.clear();
		}
		scene.numFrames = 0;
		scene.animations.clear();
	}

	Model *toModel(Scene &&scene)
	{
		checkFrames(scene);
		dropStillFrames(scene);

		Conversion convert{scene.axisConversion, scene.unitScale};
		std::unique_ptr<Model> model(scene.skinned ? toSkeletal(scene, convert) : toMorph(scene, convert));

		for (const auto &source : scene.animations)
		{
			if (source.startFrame < 0 || source.numFrames < 0 || source.startFrame + source.numFrames > scene.numFrames)
			{
				throw mt_ex("Animation " + source.name + " points outside the frames");
			}

			model::Animation animation;
			animation.name = source.name;
			animation.startFrame = source.startFrame;
			animation.numFrames = source.numFrames;
			animation.fps = source.fps;
			model->animations.push_back(animation);
		}

		return model.release();
	}
}
