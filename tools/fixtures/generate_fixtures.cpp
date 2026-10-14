/*
 * Generates the test models and textures in data/models/{md3,mdc,mdm,players}, the ET test character
 * (data/characters, data/animations) and the FBX cube and cone from scratch, so the test data can be
 * distributed freely (MIT, like the rest of ModelTool). Every shape, animation and texture is made up here.
 *
 *   cmake --build build --target generate_fixtures
 *   ./build/generate_fixtures data
 *
 * The output is deterministic: running it again gives the same files.
 */

#include "et_armature.h"
#include "idtech.h"
#include "lod.h"
#include "model_md3.h"
#include "model_mdm.h"
#include "model_mdx.h"
#include "model_scripts.h"
#include "model_ms3d.h"
#include "morph_model.h"
#include "scene_import.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <array>
#include <iomanip>
#include <map>
#include <sstream>

using namespace mt;
using namespace mt::model;

namespace {

	const float PI = glm::pi<float>();

	glm::quat rotation(float degrees, const glm::vec3 &axis)
	{
		return glm::angleAxis(glm::radians(degrees), glm::normalize(axis));
	}

	/*
	==============================================================================
	Geometry: convex primitives with outward idTech (clockwise) triangles
	==============================================================================
	*/

	/*! Vertex of a generated shape */
	struct ShapeVertex {
		glm::vec3 pos{};
		glm::vec3 normal{};
		glm::vec2 tex{};
		float along = 0; ///< 0 at the start of a tube, 1 at the end (skinning blends at the start)
	};

	/*! Convex shape, the triangles point outwards from the center */
	struct Shape {
		std::vector<ShapeVertex> vertices{};
		std::vector<glm::ivec3> tris{};
		glm::vec3 center{};

		/**
		 * Add a triangle with the idTech winding: the normal (v3 - v1) x (v2 - v1) points away from the center
		 */
		void triangle(int a, int b, int c)
		{
			const glm::vec3 &pa = vertices[size_t(a)].pos, &pb = vertices[size_t(b)].pos, &pc = vertices[size_t(c)].pos;
			const glm::vec3 normal = glm::cross(pc - pa, pb - pa);
			const glm::vec3 outward = (pa + pb + pc) / 3.0f - center;
			if (glm::dot(normal, outward) < 0)
			{
				std::swap(b, c);
			}
			tris.emplace_back(a, b, c);
		}
	};

	/**
	 * Box with flat faces
	 * @param center middle of the box
	 * @param half half of the size on each axis
	 * @param orientation rotation of the box
	 */
	Shape box(const glm::vec3 &center, const glm::vec3 &half, const glm::quat &orientation = glm::quat{1, 0, 0, 0})
	{
		Shape shape;
		shape.center = center;
		for (int axis = 0; axis < 3; axis++)
		{
			for (float side : {-1.0f, 1.0f})
			{
				const int u = (axis + 1) % 3, v = (axis + 2) % 3;
				glm::vec3 normal{0.0f};
				normal[axis] = side;
				const int start = int(shape.vertices.size());
				for (int corner = 0; corner < 4; corner++)
				{
					glm::vec3 local{0.0f};
					local[axis] = side * half[axis];
					local[u] = (corner & 1 ? 1.0f : -1.0f) * half[u];
					local[v] = (corner & 2 ? 1.0f : -1.0f) * half[v];
					shape.vertices.push_back({center + orientation * local,
											  orientation * normal,
											  {corner & 1 ? 1.0f : 0.0f, corner & 2 ? 1.0f : 0.0f},
											  (local.z + half.z) / (2 * half.z)});
				}
				shape.triangle(start, start + 1, start + 3);
				shape.triangle(start, start + 3, start + 2);
			}
		}
		return shape;
	}

	/**
	 * Tube (truncated cone) with closed ends
	 * @param from start of the axis
	 * @param to end of the axis
	 * @param startRadius radius at the start
	 * @param endRadius radius at the end
	 * @param sides number of sides
	 */
	Shape tube(const glm::vec3 &from, const glm::vec3 &to, float startRadius, float endRadius, int sides)
	{
		Shape shape;
		shape.center = (from + to) * 0.5f;
		const glm::vec3 axis = glm::normalize(to - from);
		const glm::vec3 helper = std::abs(axis.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0};
		const glm::vec3 side = glm::normalize(glm::cross(axis, helper));
		const glm::vec3 up = glm::cross(side, axis);
		const float slope = (startRadius - endRadius) / glm::length(to - from);

		// Side rings, the seam vertex is doubled for the texture coordinates
		for (int ring = 0; ring < 2; ring++)
		{
			const glm::vec3 &base = ring ? to : from;
			const float radius = ring ? endRadius : startRadius;
			for (int i = 0; i <= sides; i++)
			{
				const float angle = 2 * PI * float(i) / float(sides);
				const glm::vec3 direction = side * std::cos(angle) + up * std::sin(angle);
				shape.vertices.push_back({base + direction * radius,
										  glm::normalize(direction + axis * slope),
										  {float(i) / float(sides), float(ring)},
										  float(ring)});
			}
		}
		for (int i = 0; i < sides; i++)
		{
			const int a = i, b = i + 1, c = sides + 1 + i, d = sides + 2 + i;
			shape.triangle(a, b, d);
			shape.triangle(a, d, c);
		}

		// End caps
		for (int ring = 0; ring < 2; ring++)
		{
			const glm::vec3 &base = ring ? to : from;
			const float radius = ring ? endRadius : startRadius;
			const glm::vec3 normal = ring ? axis : -axis;
			const int centerVertex = int(shape.vertices.size());
			shape.vertices.push_back({base, normal, {0.5f, 0.5f}, float(ring)});
			for (int i = 0; i < sides; i++)
			{
				const float angle = 2 * PI * float(i) / float(sides);
				const glm::vec3 direction = side * std::cos(angle) + up * std::sin(angle);
				shape.vertices.push_back(
					{base + direction * radius, normal, {0.5f + 0.5f * std::cos(angle), 0.5f + 0.5f * std::sin(angle)}, float(ring)});
			}
			for (int i = 0; i < sides; i++)
			{
				shape.triangle(centerVertex, centerVertex + 1 + i, centerVertex + 1 + (i + 1) % sides);
			}
		}
		return shape;
	}

	/**
	 * Sphere from latitude rings
	 */
	Shape sphere(const glm::vec3 &center, float radius, int rings, int sides)
	{
		Shape shape;
		shape.center = center;
		for (int ring = 0; ring <= rings; ring++)
		{
			const float latitude = PI * float(ring) / float(rings) - PI / 2;
			for (int i = 0; i <= sides; i++)
			{
				const float angle = 2 * PI * float(i) / float(sides);
				const glm::vec3 direction{std::cos(latitude) * std::cos(angle), std::cos(latitude) * std::sin(angle), std::sin(latitude)};
				shape.vertices.push_back(
					{center + direction * radius, direction, {float(i) / float(sides), float(ring) / float(rings)}, 0});
			}
		}
		for (int ring = 0; ring < rings; ring++)
		{
			for (int i = 0; i < sides; i++)
			{
				const int a = ring * (sides + 1) + i, b = a + 1, c = a + sides + 1, d = c + 1;
				if (ring > 0)
				{
					shape.triangle(a, b, d);
				}
				if (ring < rings - 1)
				{
					shape.triangle(a, d, c);
				}
			}
		}
		return shape;
	}

	/*
	==============================================================================
	Files
	==============================================================================
	*/

	void writeText(const std::filesystem::path &path, const std::string &text)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary);
		out << text;
	}

	void writeBytes(const std::filesystem::path &path, const std::vector<byte> &data)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	}

	/**
	 * Uncompressed 24 bit TGA, the color comes from a function of the texture coordinates (0 - 1)
	 */
	void writeTga(const std::filesystem::path &path, int size, const std::function<glm::vec3(float, float)> &color)
	{
		std::vector<byte> data(18, 0);
		data[2] = 2; // uncompressed true color
		data[12] = byte(size & 255);
		data[13] = byte(size >> 8);
		data[14] = byte(size & 255);
		data[15] = byte(size >> 8);
		data[16] = 24;
		for (int y = 0; y < size; y++)
		{
			for (int x = 0; x < size; x++)
			{
				glm::vec3 c = glm::clamp(color((float(x) + 0.5f) / float(size), (float(y) + 0.5f) / float(size)), 0.0f, 1.0f);
				data.push_back(byte(std::lround(c.b * 255)));
				data.push_back(byte(std::lround(c.g * 255)));
				data.push_back(byte(std::lround(c.r * 255)));
			}
		}
		writeBytes(path, data);
	}

	/*
	==============================================================================
	Mannequin: ET skeletal model (MDM + MDX + aninc) on the default ET armature
	==============================================================================
	*/

	/*! Bind pose position of a default armature bone, in game units (Z up, X forward, Y left) */
	glm::vec3 bonePosition(const std::string &name)
	{
		const float s = name.find(" R ") != std::string::npos ? -1.0f : 1.0f;
		static const std::map<std::string, glm::vec3> center = {
			{"Bip01 Pelvis", {0, 0, 0}},  {"Bip01 Spine", {0, 0, 1.5f}}, {"Bip01 Spine1", {0, 0, 5}}, {"Bip01 Spine2", {0, 0, 9.5f}},
			{"Bip01 Spine3", {0, 0, 14}}, {"Bip01 Neck", {0, 0, 21}},	 {"Bip01 Head", {0, 0, 24}},
		};
		if (auto found = center.find(name); found != center.end())
		{
			return found->second;
		}

		// Sided bones, the left side, mirrored for the right
		const std::string part = name.substr(8);
		static const std::map<std::string, glm::vec3> sided = {
			{"Clavicle", {0, 1.5f, 20.5f}},	   {"UpperArm", {0, 6.5f, 20}},
			{"Forearm", {0, 7.5f, 9.5f}},	   {"Hand", {0, 8, 0}},
			{"Finger0", {1.4f, 8.3f, -1}},	   {"Finger01", {1.9f, 8.4f, -2.3f}},
			{"Finger02", {2.2f, 8.4f, -3.4f}}, {"Thigh", {0, 3.5f, -1.5f}},
			{"Calf", {0.3f, 3.8f, -12}},	   {"Foot", {0, 4, -22}},
			{"Toe0", {4, 4, -23.5f}},
		};
		if (auto found = sided.find(part); found != sided.end())
		{
			return found->second * glm::vec3{1, s, 1};
		}

		// Fingers 1 - 4 in a row, three joints each
		if (part.size() >= 7 && part.compare(0, 6, "Finger") == 0)
		{
			const int finger = part[6] - '0';
			const int joint = part.size() > 7 ? part[7] - '0' : 0;
			const float x = 0.9f - float(finger - 1) * 0.6f;
			const float z = joint == 0 ? -2.4f : (joint == 1 ? -4.0f : -5.2f);
			return {x, 8.1f * s, z};
		}
		throw mt_ex("No position for bone " + name);
	}

	/*! Tag of the mannequin: name, parent bone and world position in the bind pose */
	struct TagDefinition {
		const char *name;
		const char *bone;
		glm::vec3 position;
		glm::quat orientation;
	};

	const std::vector<TagDefinition> &mannequinTags()
	{
		const glm::quat none{1, 0, 0, 0};
		static const std::vector<TagDefinition> tags = {
			{"tag_head", "Bip01 Head", {0, 0, 24}, none},
			{"tag_weapon2", "Bip01 L Hand", {1, 8.2f, -2.5f}, rotation(-90, {0, 1, 0})},
			{"tag_armleft", "Bip01 L UpperArm", {0, 7.5f, 15}, none},
			{"tag_weapon", "Bip01 R Hand", {1, -8.2f, -2.5f}, rotation(-90, {0, 1, 0})},
			{"tag_armright", "Bip01 R UpperArm", {0, -7.5f, 15}, none},
			{"tag_back", "Bip01 Spine3", {-3.5f, 0, 15}, rotation(180, {0, 0, 1})},
			{"tag_chest", "Bip01 Spine3", {3.5f, 0, 14}, none},
			{"tag_bright", "Bip01 Spine", {0, -4.5f, 0}, none},
			{"tag_lbelt", "Bip01 Spine", {0, 0, 0}, none},
			{"tag_footright", "Bip01 R Foot", {1, -4, -23}, none},
			{"tag_legright", "Bip01 R Thigh", {0, -4, -6}, none},
			{"tag_mouth", "Bip01 Head", {2.5f, 0, 25.5f}, none},
			{"tag_bleft", "Bip01 Spine1", {0, 4.5f, 1}, none},
			{"tag_ubelt", "Bip01 Spine", {0, 0, 3}, none},
			{"tag_footleft", "Bip01 L Foot", {1, 4, -23}, none},
			{"tag_legleft", "Bip01 L Thigh", {0, 4, -6}, none},
			{"tag_torso", "Bip01 Spine", {0, 0, 1.5f}, none},
		};
		return tags;
	}

	/*! Animation sequence of the mannequin */
	struct Sequence {
		const char *name;
		int start;
		int frames;
		bool loop;
	};

	const std::vector<Sequence> &mannequinSequences()
	{
		static const std::vector<Sequence> sequences = {
			{"idle", 0, 20, true},	   {"walk", 20, 24, true},	 {"run", 44, 16, true},	 {"wave", 60, 20, false},
			{"crouch", 80, 10, false}, {"death", 90, 26, false}, {"dead", 115, 1, true},
		};
		return sequences;
	}

	const int MANNEQUIN_FRAMES = 116;

	/**
	 * Local rotations of the bones (relative to the bind pose) and the pelvis offset in a frame
	 */
	void mannequinPose(int frame, const std::vector<std::string> &names, std::vector<glm::quat> &local, glm::vec3 &pelvis)
	{
		local.assign(names.size(), glm::quat{1, 0, 0, 0});
		pelvis = {0, 0, 0};
		auto bone = [&](const std::string &name) -> glm::quat & {
			auto found = std::find(names.begin(), names.end(), name);
			return local[size_t(found - names.begin())];
		};
		auto spin
			= [&](const std::string &name, float degrees, const glm::vec3 &axis) { bone(name) = rotation(degrees, axis) * bone(name); };

		// The fingers curl a bit in every pose
		for (const auto &name : names)
		{
			if (name.find("Finger") != std::string::npos && name.size() > 15)
			{
				spin(name, 12, {name.find(" R ") != std::string::npos ? -1.0f : 1.0f, 0, 0});
			}
		}

		const glm::vec3 pitch{0, 1, 0}, roll{1, 0, 0}, yaw{0, 0, 1};
		auto legs = [&](float left, float right, float knees) {
			spin("Bip01 L Thigh", left, pitch);
			spin("Bip01 R Thigh", right, pitch);
			spin("Bip01 L Calf", std::max(0.0f, -left) * knees, pitch);
			spin("Bip01 R Calf", std::max(0.0f, -right) * knees, pitch);
		};

		for (const auto &sequence : mannequinSequences())
		{
			if (frame < sequence.start || frame >= sequence.start + sequence.frames)
			{
				continue;
			}
			const float t = sequence.frames > 1 ? float(frame - sequence.start) / float(sequence.frames - 1) : 1.0f;
			const float cycle = 2 * PI * float(frame - sequence.start) / float(sequence.frames);
			const std::string name = sequence.name;

			if (name == "idle")
			{
				spin("Bip01 Spine1", 3 * std::sin(cycle), roll);
				spin("Bip01 Head", 8 * std::sin(cycle), yaw);
				spin("Bip01 L UpperArm", 5 + 2 * std::sin(cycle), roll);
				spin("Bip01 R UpperArm", -5 - 2 * std::sin(cycle), roll);
			}
			else if (name == "walk" || name == "run")
			{
				const float amount = name == "run" ? 40.0f : 25.0f;
				legs(-amount * std::sin(cycle), amount * std::sin(cycle), 1.4f);
				spin("Bip01 L UpperArm", amount * 0.8f * std::sin(cycle), pitch);
				spin("Bip01 R UpperArm", -amount * 0.8f * std::sin(cycle), pitch);
				spin("Bip01 L Forearm", -20, pitch);
				spin("Bip01 R Forearm", -20, pitch);
				spin("Bip01 Spine", name == "run" ? 12.0f : 4.0f, pitch);
				spin("Bip01 Spine2", 6 * std::sin(cycle), yaw);
				pelvis = {name == "run" ? 1.0f : 0.0f, 0, -std::abs(std::sin(cycle)) * (name == "run" ? 1.5f : 0.6f)};
			}
			else if (name == "wave")
			{
				spin("Bip01 R UpperArm", -150 * std::min(t * 3, 1.0f), roll);
				spin("Bip01 R Forearm", 30 * std::sin(cycle * 2), roll);
				spin("Bip01 Head", -15 * std::min(t * 3, 1.0f), yaw);
			}
			else if (name == "crouch")
			{
				legs(-70 * t, -70 * t, 1.9f);
				spin("Bip01 Spine", 25 * t, pitch);
				pelvis = {-2 * t, 0, -8 * t};
			}
			else if (name == "death" || name == "dead")
			{
				const float fall = name == "dead" ? 1.0f : std::min(t * 1.3f, 1.0f);
				spin("Bip01 Pelvis", -85 * fall, pitch);
				spin("Bip01 L UpperArm", 70 * fall, roll);
				spin("Bip01 R UpperArm", -70 * fall, roll);
				spin("Bip01 Head", 20 * fall, pitch);
				legs(-15 * fall, 10 * fall, 1.0f);
				pelvis = {-10 * fall, 0, -19 * fall};
			}
		}
	}

	/**
	 * Skinned tube from a bone to a point, the start blends with the parent bone
	 */
	void addLimb(import::Mesh &mesh, const Shape &shape, int bone, int parent)
	{
		const int start = int(mesh.vertices.size());
		for (const auto &vert : shape.vertices)
		{
			import::Vertex out;
			out.pos = vert.pos;
			out.normal = vert.normal;
			out.tex = vert.tex;
			if (parent >= 0 && vert.along < 0.5f)
			{
				out.influences = {{bone, 0.6f}, {parent, 0.4f}};
			}
			else
			{
				out.influences = {{bone, 1.0f}};
			}
			mesh.vertices.push_back(out);
		}
		for (const auto &tri : shape.tris)
		{
			mesh.tris.push_back(tri + glm::ivec3(start));
		}
	}

	/**
	 * The mannequin as an imported scene: the bones, the tags as tag bones (after the bones), the meshes and the frames
	 */
	import::Scene mannequinScene()
	{
		const auto &armature = et::defaultArmature();
		std::vector<std::string> names;
		for (const auto &bone : armature)
		{
			names.emplace_back(bone.name);
		}
		auto index = [&](const std::string &name) { return int(std::find(names.begin(), names.end(), name) - names.begin()); };

		import::Scene scene;
		scene.name = "models/players/mannequin/body.mdm";
		scene.skinned = true;
		scene.numFrames = MANNEQUIN_FRAMES;

		// Bones and their frames: the bind pose has no rotations, the frames rotate every bone around its joint
		std::vector<glm::vec3> positions;
		for (size_t i = 0; i < armature.size(); i++)
		{
			import::Node node;
			node.name = names[i];
			node.parent = armature[i].parentId;
			positions.push_back(bonePosition(names[i]));
			node.bind = glm::translate(positions.back());
			scene.nodes.push_back(node);
		}
		for (int frame = 0; frame < MANNEQUIN_FRAMES; frame++)
		{
			std::vector<glm::quat> local;
			glm::vec3 pelvis;
			mannequinPose(frame, names, local, pelvis);
			for (size_t i = 0; i < armature.size(); i++)
			{
				auto &node = scene.nodes[i];
				const glm::mat4 parent = node.parent < 0 ? glm::translate(pelvis) : scene.nodes[size_t(node.parent)].frames.back();
				const glm::vec3 offset = node.parent < 0 ? positions[i] : positions[i] - positions[size_t(node.parent)];
				node.frames.push_back(parent * glm::translate(offset) * glm::toMat4(local[i]));
			}
		}

		// Tags as tag bones, baked into bone tags after the conversion
		for (const auto &tag : mannequinTags())
		{
			import::Node node;
			node.name = tag.name;
			node.parent = index(tag.bone);
			const glm::vec3 offset = tag.position - positions[size_t(node.parent)];
			node.bind = glm::translate(tag.position) * glm::toMat4(tag.orientation);
			for (int frame = 0; frame < MANNEQUIN_FRAMES; frame++)
			{
				node.frames.push_back(scene.nodes[size_t(node.parent)].frames[size_t(frame)] * glm::translate(offset)
									  * glm::toMat4(tag.orientation));
			}
			scene.tags.push_back(int(scene.nodes.size()));
			scene.nodes.push_back(node);
		}

		// Meshes: tubes along the bones
		auto limb = [&](import::Mesh &mesh, const std::string &boneName, const glm::vec3 &end, float r0, float r1, int sides = 8) {
			const int bone = index(boneName);
			addLimb(mesh, tube(positions[size_t(bone)], end, r0, r1, sides), bone, armature[size_t(bone)].parentId);
		};
		auto at = [&](const std::string &name) { return positions[size_t(index(name))]; };

		import::Mesh body;
		body.name = "u_body";
		addLimb(body, box({0, 0, 0}, {2.8f, 4.5f, 2.5f}), index("Bip01 Pelvis"), -1);
		limb(body, "Bip01 Spine", at("Bip01 Spine1"), 4.2f, 4.0f);
		limb(body, "Bip01 Spine1", at("Bip01 Spine2"), 4.0f, 4.4f);
		limb(body, "Bip01 Spine2", at("Bip01 Spine3"), 4.4f, 5.0f);
		addLimb(body, box(at("Bip01 Spine3") + glm::vec3{0, 0, 3.5f}, {3.2f, 5.6f, 3.5f}), index("Bip01 Spine3"), index("Bip01 Spine2"));
		limb(body, "Bip01 Neck", at("Bip01 Head"), 1.5f, 1.4f, 6);
		for (const char *side : {"L", "R"})
		{
			const std::string s = std::string("Bip01 ") + side + " ";
			limb(body, s + "Clavicle", at(s + "UpperArm"), 1.6f, 1.8f, 6);
			limb(body, s + "UpperArm", at(s + "Forearm"), 1.8f, 1.5f);
			limb(body, s + "Forearm", at(s + "Hand"), 1.5f, 1.2f);
		}

		import::Mesh legs;
		legs.name = "l_legs";
		for (const char *side : {"L", "R"})
		{
			const std::string s = std::string("Bip01 ") + side + " ";
			limb(legs, s + "Thigh", at(s + "Calf"), 2.4f, 1.9f);
			limb(legs, s + "Calf", at(s + "Foot"), 1.9f, 1.4f);
			addLimb(legs, box(at(s + "Foot") + glm::vec3{1.2f, 0, -0.9f}, {2.6f, 1.4f, 0.9f}), index(s + "Foot"), index(s + "Calf"));
			limb(legs, s + "Toe0", at(s + "Toe0") + glm::vec3{1.5f, 0, 0}, 1.0f, 0.8f, 6);
		}

		auto hand = [&](const char *side, const char *meshName) {
			import::Mesh mesh;
			mesh.name = meshName;
			const std::string s = std::string("Bip01 ") + side + " ";
			addLimb(mesh, box(at(s + "Hand") + glm::vec3{0, 0, -1}, {1.6f, 0.6f, 1.4f}), index(s + "Hand"), index(s + "Forearm"));
			for (const char *finger : {"0", "1", "2", "3", "4"})
			{
				const std::string base = s + "Finger" + finger;
				std::vector<std::string> chain = finger[0] == '0' ? std::vector<std::string>{base, s + "Finger01", s + "Finger02"}
																  : std::vector<std::string>{base, base + "1", base + "2"};
				for (size_t j = 0; j < chain.size(); j++)
				{
					const glm::vec3 end = j + 1 < chain.size() ? at(chain[j + 1]) : at(chain[j]) + glm::vec3{0, 0, -1.0f};
					limb(mesh, chain[j], end, 0.35f, 0.3f, 5);
				}
			}
			return mesh;
		};

		scene.meshes.push_back(body);
		scene.meshes.push_back(hand("L", "u_lfthand"));
		scene.meshes.push_back(legs);
		scene.meshes.push_back(hand("R", "u_rthand"));
		return scene;
	}

	void generateMannequin(const std::filesystem::path &folder)
	{
		const auto &armature = et::defaultArmature();
		std::unique_ptr<Model> converted(import::toModel(mannequinScene()));
		auto &model = dynamic_cast<skeletal::SkeletalModel &>(*converted);

		if (skeletal::bakeTags(model) != int(mannequinTags().size()) || model.joints.size() != armature.size())
		{
			throw mt_ex("The mannequin tags did not bake into bone tags");
		}

		// Back to the order of the definitions (baking adds them from the last bone)
		auto order = [](const skeletal::BoneTag &tag) {
			const auto &tags = mannequinTags();
			return std::find_if(tags.begin(), tags.end(), [&](const TagDefinition &definition) { return tag.name == definition.name; })
				   - tags.begin();
		};
		std::stable_sort(model.tags.begin(), model.tags.end(), [&](const auto &a, const auto &b) { return order(a) < order(b); });

		model.currentFrame = 0;
		if (!calculateLod(model))
		{
			throw mt_ex("Could not calculate the mannequin levels of detail");
		}

		for (const auto &sequence : mannequinSequences())
		{
			Animation animation;
			animation.name = sequence.name;
			animation.startFrame = sequence.start;
			animation.numFrames = sequence.frames;
			animation.fps = 20;
			animation.loop = sequence.loop;
			animation.moveSpeed = std::string(sequence.name) == "walk" ? 60 : (std::string(sequence.name) == "run" ? 160 : 0);
			animation.transition = 100;
			model.animations.push_back(animation);
		}

		writeMDM(model, (folder / "mannequin.mdm").string());
		writeMDX(model, (folder / "mannequin.mdx").string());
		writeAninc(model, (folder / "mannequin.aninc").string());

		// The second file of the ET character's animation group: the wave frames on their own
		const auto data = folder.parent_path().parent_path();
		std::filesystem::create_directories(data / "animations/mannequin");
		model.firstExportFrame = 60;
		model.lastExportFrame = 79;
		writeMDX(model, (data / "animations/mannequin/salute.mdx").string());
	}

	/*
	==============================================================================
	MS3D (MilkShape 3D): the mannequin with the tags as bones, 96 frames
	==============================================================================
	*/

	void generateMs3d(const std::filesystem::path &path)
	{
		// The scene keeps the tags as tag bones, they are written as bones
		std::unique_ptr<Model> converted(import::toModel(mannequinScene()));
		auto &model = dynamic_cast<skeletal::SkeletalModel &>(*converted);

		// Every bone followed by its tag bones, then its children: not the default ET order (the tests reorganize it)
		std::vector<int> order;
		std::function<void(int)> visit = [&](int bone) {
			order.push_back(bone);
			for (size_t i = 0; i < model.joints.size(); i++)
			{
				if (model.joints[i].parentId == bone && model.joints[i].asTag)
				{
					order.push_back(int(i));
				}
			}
			for (size_t i = 0; i < model.joints.size(); i++)
			{
				if (model.joints[i].parentId == bone && !model.joints[i].asTag)
				{
					visit(int(i));
				}
			}
		};
		visit(0);
		skeletal::reorderJoints(model, order);

		model.firstExportFrame = 0;
		model.lastExportFrame = 95;
		model.animations = {{"test", 0, 96, 20}};
		writeMS3D(model, path.string());
	}

	/*
	==============================================================================
	FBX: ASCII FBX 7.4 files for the static import tests (ufbx reads ASCII FBX)
	==============================================================================
	*/

	/*! Polygon mesh for an FBX file: counter clockwise polygons (FBX / OpenGL winding) */
	struct FbxMesh {
		std::vector<glm::vec3> positions{};
		std::vector<std::vector<int>> polygons{};
		std::vector<std::vector<glm::vec3>> normals{}; ///< per polygon corner
		std::vector<std::vector<glm::vec2>> uvs{};	   ///< per polygon corner
	};

	template<typename T, typename F>
	std::string fbxArray(const std::vector<T> &values, F &&format)
	{
		std::string text = "*" + std::to_string(values.size()) + " {\n\t\t\ta: ";
		for (size_t i = 0; i < values.size(); i++)
		{
			text += (i ? "," : "") + format(values[i]);
		}
		return text + "\n\t\t}";
	}

	std::string fbxNumber(double value)
	{
		std::ostringstream text;
		text << std::setprecision(9) << value;
		return text.str();
	}

	/**
	 * ASCII FBX with one mesh node, optionally an animation take that keeps the node where it is
	 * @param name node and geometry name
	 * @param mesh mesh
	 * @param takeFrames frames of the take (24 fps), 0 for none
	 */
	std::string fbxFile(const std::string &name, const FbxMesh &mesh, int takeFrames)
	{
		std::vector<double> vertices, normals, uvs;
		std::vector<int> indices, uvIndices;
		for (const auto &position : mesh.positions)
		{
			vertices.insert(vertices.end(), {position.x, position.y, position.z});
		}
		for (size_t p = 0; p < mesh.polygons.size(); p++)
		{
			const auto &polygon = mesh.polygons[p];
			for (size_t c = 0; c < polygon.size(); c++)
			{
				// The last corner of a polygon is stored as -(index + 1)
				indices.push_back(c + 1 == polygon.size() ? -polygon[c] - 1 : polygon[c]);
				const glm::vec3 &normal = mesh.normals[p][c];
				normals.insert(normals.end(), {normal.x, normal.y, normal.z});
				const glm::vec2 &uv = mesh.uvs[p][c];
				uvIndices.push_back(int(uvs.size() / 2));
				uvs.insert(uvs.end(), {uv.x, uv.y});
			}
		}

		auto number = [](double value) { return fbxNumber(value); };
		auto integer = [](int value) { return std::to_string(value); };
		std::string text
			= "; FBX 7.4.0 project file\n"
			  "; Generated by tools/fixtures/generate_fixtures.cpp (ModelTool, MIT)\n\n"
			  "FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n\tCreator: \"ModelTool fixture generator\"\n}\n"
			  "GlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n"
			  "\t\tP: \"UpAxis\", \"int\", \"Integer\", \"\",2\n"
			  "\t\tP: \"UpAxisSign\", \"int\", \"Integer\", \"\",1\n"
			  "\t\tP: \"FrontAxis\", \"int\", \"Integer\", \"\",1\n"
			  "\t\tP: \"FrontAxisSign\", \"int\", \"Integer\", \"\",-1\n"
			  "\t\tP: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n"
			  "\t\tP: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n"
			  "\t\tP: \"UnitScaleFactor\", \"double\", \"Number\", \"\",1\n"
			  "\t\tP: \"TimeMode\", \"enum\", \"\", \"\",11\n"
			  "\t}\n}\n\n"
			  "Objects:  {\n";
		text += "\tGeometry: 1000, \"Geometry::" + name + "\", \"Mesh\" {\n";
		text += "\t\tVertices: " + fbxArray(vertices, number) + "\n";
		text += "\t\tPolygonVertexIndex: " + fbxArray(indices, integer) + "\n";
		text += "\t\tGeometryVersion: 124\n";
		text += "\t\tLayerElementNormal: 0 {\n\t\t\tVersion: 102\n\t\t\tName: \"\"\n"
				"\t\t\tMappingInformationType: \"ByPolygonVertex\"\n\t\t\tReferenceInformationType: \"Direct\"\n";
		text += "\t\t\tNormals: " + fbxArray(normals, number) + "\n\t\t}\n";
		text += "\t\tLayerElementUV: 0 {\n\t\t\tVersion: 101\n\t\t\tName: \"UVMap\"\n"
				"\t\t\tMappingInformationType: \"ByPolygonVertex\"\n\t\t\tReferenceInformationType: \"IndexToDirect\"\n";
		text += "\t\t\tUV: " + fbxArray(uvs, number) + "\n";
		text += "\t\t\tUVIndex: " + fbxArray(uvIndices, integer) + "\n\t\t}\n";
		text += "\t\tLayer: 0 {\n\t\t\tVersion: 100\n"
				"\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementNormal\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n"
				"\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementUV\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t}\n";
		text += "\t}\n";
		text += "\tModel: 2000, \"Model::" + name
				+ "\", \"Mesh\" {\n\t\tVersion: 232\n\t\tProperties70:  {\n\t\t}\n"
				  "\t\tShading: T\n\t\tCulling: \"CullingOff\"\n\t}\n";
		text += "\tMaterial: 3000, \"Material::" + name
				+ "_material\", \"\" {\n\t\tVersion: 102\n\t\tShadingModel: \"phong\"\n"
				  "\t\tMultiLayer: 0\n\t\tProperties70:  {\n\t\t}\n\t}\n";

		std::string connections = "Connections:  {\n\tC: \"OO\",2000,0\n\tC: \"OO\",1000,2000\n\tC: \"OO\",3000,2000\n";
		if (takeFrames > 0)
		{
			// A take with translation keys that stay at the same place
			const long long second = 46186158000LL;
			const long long stop = second * (takeFrames - 1) / 24;
			text += "\tAnimationStack: 4000, \"AnimStack::Take 001\", \"\" {\n\t\tProperties70:  {\n"
					"\t\t\tP: \"LocalStart\", \"KTime\", \"Time\", \"\",0\n"
					"\t\t\tP: \"ReferenceStart\", \"KTime\", \"Time\", \"\",0\n"
					"\t\t\tP: \"LocalStop\", \"KTime\", \"Time\", \"\","
					+ std::to_string(stop) + "\n\t\t\tP: \"ReferenceStop\", \"KTime\", \"Time\", \"\"," + std::to_string(stop)
					+ "\n\t\t}\n\t}\n";
			text += "\tAnimationLayer: 4001, \"AnimLayer::BaseLayer\", \"\" {\n\t}\n";
			text += "\tAnimationCurveNode: 4002, \"AnimCurveNode::T\", \"\" {\n\t\tProperties70:  {\n"
					"\t\t\tP: \"d|X\", \"Number\", \"\", \"A\",0\n\t\t\tP: \"d|Y\", \"Number\", \"\", \"A\",0\n"
					"\t\t\tP: \"d|Z\", \"Number\", \"\", \"A\",0\n\t\t}\n\t}\n";
			connections += "\tC: \"OO\",4001,4000\n\tC: \"OO\",4002,4001\n\tC: \"OP\",4002,2000, \"Lcl Translation\"\n";
			const char *axes[3] = {"d|X", "d|Y", "d|Z"};
			for (int axis = 0; axis < 3; axis++)
			{
				const int id = 4003 + axis;
				text += "\tAnimationCurve: " + std::to_string(id)
						+ ", \"AnimCurve::\", \"\" {\n\t\tDefault: 0\n\t\tKeyVer: 4008\n"
						  "\t\tKeyTime: *2 {\n\t\t\ta: 0,"
						+ std::to_string(stop)
						+ "\n\t\t}\n\t\tKeyValueFloat: *2 {\n\t\t\ta: 0,0\n\t\t}\n"
						  "\t\tKeyAttrFlags: *1 {\n\t\t\ta: 24836\n\t\t}\n\t\tKeyAttrDataFloat: *4 {\n\t\t\ta: 0,0,255790911,0\n\t\t}\n"
						  "\t\tKeyAttrRefCount: *1 {\n\t\t\ta: 2\n\t\t}\n\t}\n";
				connections += "\tC: \"OP\"," + std::to_string(id) + ",4002, \"" + axes[axis] + "\"\n";
			}
		}
		text += "}\n\n" + connections + "}\n";
		return text;
	}

	/*! Cube 2 units wide, a quad per side (24 corners with their own normals) */
	FbxMesh fbxCube()
	{
		FbxMesh mesh;
		for (int corner = 0; corner < 8; corner++)
		{
			mesh.positions.push_back({corner & 1 ? 1.0f : -1.0f, corner & 2 ? 1.0f : -1.0f, corner & 4 ? 1.0f : -1.0f});
		}
		for (int axis = 0; axis < 3; axis++)
		{
			for (int side : {0, 1})
			{
				const int u = (axis + 1) % 3, v = (axis + 2) % 3;
				std::vector<int> quad;
				for (const auto &[a, b] : std::vector<std::pair<int, int>>{{0, 0}, {1, 0}, {1, 1}, {0, 1}})
				{
					quad.push_back((side << axis) | (a << u) | (b << v));
				}
				// Counter clockwise seen from outside: the normal of the first corners points out
				glm::vec3 normal{0.0f};
				normal[axis] = side ? 1.0f : -1.0f;
				const glm::vec3 &p0 = mesh.positions[size_t(quad[0])], &p1 = mesh.positions[size_t(quad[1])],
								&p2 = mesh.positions[size_t(quad[2])];
				if (glm::dot(glm::cross(p1 - p0, p2 - p0), normal) < 0)
				{
					std::reverse(quad.begin(), quad.end());
				}
				mesh.polygons.push_back(quad);
				mesh.normals.push_back(std::vector<glm::vec3>(4, normal));
				mesh.uvs.push_back({{0, 0}, {1, 0}, {1, 1}, {0, 1}});
			}
		}
		return mesh;
	}

	/*! Cone: a base polygon and side triangles */
	FbxMesh fbxCone(int sides)
	{
		FbxMesh mesh;
		for (int i = 0; i < sides; i++)
		{
			const float angle = 2 * PI * float(i) / float(sides);
			mesh.positions.push_back({std::cos(angle), std::sin(angle), 0});
		}
		mesh.positions.push_back({0, 0, 2});
		const int apex = sides;

		std::vector<int> base;
		std::vector<glm::vec2> baseUv;
		for (int i = sides - 1; i >= 0; i--)
		{
			base.push_back(i);
			baseUv.push_back(glm::vec2(mesh.positions[size_t(i)]) * 0.5f + 0.5f);
		}
		mesh.polygons.push_back(base);
		mesh.normals.push_back(std::vector<glm::vec3>(size_t(sides), {0, 0, -1}));
		mesh.uvs.push_back(baseUv);

		for (int i = 0; i < sides; i++)
		{
			const int next = (i + 1) % sides;
			const glm::vec3 &a = mesh.positions[size_t(i)], &b = mesh.positions[size_t(next)];
			const glm::vec3 normal = glm::normalize(glm::cross(b - a, mesh.positions[size_t(apex)] - a));
			mesh.polygons.push_back({i, next, apex});
			mesh.normals.push_back({normal, normal, normal});
			mesh.uvs.push_back({{float(i) / float(sides), 0}, {float(i + 1) / float(sides), 0}, {(float(i) + 0.5f) / float(sides), 1}});
		}
		return mesh;
	}

	/*
	==============================================================================
	Morph models (MD3): rigid parts moved per frame
	==============================================================================
	*/

	/*! Shape that moves with a part */
	struct PartShape {
		Shape shape;
		int part;
	};

	/*! Morph mesh made of shapes on parts, a part transform per frame */
	struct MorphMeshDefinition {
		std::string name;
		std::string material;
		std::vector<PartShape> shapes;
	};

	/*! Tag on a part */
	struct MorphTagDefinition {
		std::string name;
		int part;
		glm::vec3 position;
		glm::quat orientation;
	};

	/**
	 * Build a morph model through the scene importer (it also calculates the frame bounds)
	 * @param name model name in the file
	 * @param meshes meshes
	 * @param tags tags
	 * @param frames part transforms per frame
	 */
	std::unique_ptr<morph::MorphModel>
	buildMorph(const std::string &name, const std::vector<MorphMeshDefinition> &meshes, const std::vector<MorphTagDefinition> &tags,
			   const std::vector<std::vector<glm::mat4>> &frames)
	{
		import::Scene scene;
		scene.name = name;
		scene.numFrames = int(frames.size());

		for (const auto &definition : meshes)
		{
			import::Mesh mesh;
			mesh.name = definition.name;
			mesh.material = definition.material;
			mesh.framePositions.resize(frames.size());
			mesh.frameNormals.resize(frames.size());
			for (const auto &item : definition.shapes)
			{
				const int start = int(mesh.vertices.size());
				for (const auto &vert : item.shape.vertices)
				{
					mesh.vertices.push_back({vert.pos, vert.normal, vert.tex, {}});
					for (size_t frame = 0; frame < frames.size(); frame++)
					{
						const glm::mat4 &transform = frames[frame][size_t(item.part)];
						mesh.framePositions[frame].push_back(glm::vec3(transform * glm::vec4(vert.pos, 1)));
						mesh.frameNormals[frame].push_back(glm::normalize(glm::mat3(transform) * vert.normal));
					}
				}
				for (const auto &tri : item.shape.tris)
				{
					mesh.tris.push_back(tri + glm::ivec3(start));
				}
			}
			// The bind pose is the first frame
			for (size_t v = 0; v < mesh.vertices.size(); v++)
			{
				mesh.vertices[v].pos = mesh.framePositions[0][v];
				mesh.vertices[v].normal = mesh.frameNormals[0][v];
			}
			scene.meshes.push_back(mesh);
		}

		for (const auto &tag : tags)
		{
			import::Node node;
			node.name = tag.name;
			for (const auto &frame : frames)
			{
				node.frames.push_back(frame[size_t(tag.part)] * glm::translate(tag.position) * glm::toMat4(tag.orientation));
			}
			node.bind = node.frames[0];
			scene.tags.push_back(int(scene.nodes.size()));
			scene.nodes.push_back(node);
		}

		std::unique_ptr<Model> converted(import::toModel(std::move(scene)));
		auto *morph = dynamic_cast<morph::MorphModel *>(converted.get());
		if (!morph)
		{
			throw mt_ex("Not a morph model: " + name);
		}
		converted.release();
		return std::unique_ptr<morph::MorphModel>(morph);
	}

	/*
	==============================================================================
	Tube: weapon like model with a recoil animation, MD3 and MDC
	==============================================================================
	*/

	// The tag is turned like the angles in the files (MD3 axis, MDC angles) give it
	const glm::quat TUBE_FLASH_ORIENTATION = Point::fromAxis({}, idtech::anglesToAxis({-10, 0, 0})).rotation;

	std::unique_ptr<morph::MorphModel> buildTube()
	{
		MorphMeshDefinition mesh{"w_tube", "models/weapons/tube/tube.tga", {}};
		mesh.shapes.push_back({tube({-8, 0, 0}, {22, 0, 0}, 1.3f, 1.3f, 12), 0});
		mesh.shapes.push_back({tube({22, 0, 0}, {30, 0, 0}, 3.0f, 0.6f, 12), 0});
		mesh.shapes.push_back({box({2, 0, -2.4f}, {1.2f, 0.6f, 1.6f}, rotation(-15, {0, 1, 0})), 0});
		mesh.shapes.push_back({box({8, 0, 2.0f}, {1.5f, 0.3f, 0.8f}), 0});

		// Recoil: the tube moves back and comes forward again
		std::vector<std::vector<glm::mat4>> frames;
		for (float recoil : {0.0f, -1.5f, -0.5f})
		{
			frames.push_back({glm::translate(glm::vec3{recoil, 0, 0}) * glm::toMat4(rotation(recoil * 2, {0, 1, 0}))});
		}

		return buildMorph("models/weapons/tube/tube.md3", {mesh}, {{"tag_flash", 0, {30.5f, 0, 0}, TUBE_FLASH_ORIENTATION}}, frames);
	}

	void generateTube(const std::filesystem::path &data)
	{
		auto model = buildTube();
		std::filesystem::create_directories(data / "models/md3");
		writeMD3(*model, (data / "models/md3/tube.md3").string());
		// Compressed frames, so the tests read MDC offsets
		MdcOptions options;
		options.compress = true;
		writeMDC(*model, (data / "models/mdc/tube.mdc").string(), options);
	}

	/*
	==============================================================================
	Robot: Quake 3 style player (lower, upper, head, animation.cfg, skins, textures)
	==============================================================================
	*/

	/*! animation.cfg line */
	struct PlayerSequence {
		const char *name;
		int first;
		int frames;
		int loop;
		int fps;
	};

	const std::vector<PlayerSequence> &robotSequences()
	{
		static const std::vector<PlayerSequence> sequences = {
			{"BOTH_DEATH1", 0, 10, 0, 20},	  {"BOTH_DEAD1", 9, 1, 0, 20},	  {"BOTH_DEATH2", 10, 10, 0, 20},
			{"BOTH_DEAD2", 19, 1, 0, 20},	  {"BOTH_DEATH3", 20, 10, 0, 20}, {"BOTH_DEAD3", 29, 1, 0, 20},
			{"TORSO_GESTURE", 30, 12, 0, 15}, {"TORSO_ATTACK", 42, 6, 0, 15}, {"TORSO_ATTACK2", 48, 6, 0, 15},
			{"TORSO_DROP", 54, 5, 0, 20},	  {"TORSO_RAISE", 59, 4, 0, 20},  {"TORSO_STAND", 63, 1, 0, 15},
			{"TORSO_STAND2", 64, 1, 0, 15},	  {"LEGS_WALKCR", 65, 8, 8, 20},  {"LEGS_WALK", 73, 12, 12, 20},
			{"LEGS_RUN", 85, 9, 9, 16},		  {"LEGS_BACK", 94, 10, 10, 20},  {"LEGS_SWIM", 104, 10, 10, 15},
			{"LEGS_JUMP", 114, 8, 0, 15},	  {"LEGS_LAND", 122, 3, 0, 15},	  {"LEGS_JUMPB", 125, 8, 0, 15},
			{"LEGS_LANDB", 133, 3, 0, 15},	  {"LEGS_IDLE", 136, 10, 10, 15}, {"LEGS_IDLECR", 146, 10, 10, 15},
			{"LEGS_TURN", 156, 7, 7, 15},
		};
		return sequences;
	}

	/*! Legs only frames in the file are after the torso only frames, lower.md3 does not have the torso frames */
	const int ROBOT_TORSO_FRAMES = 35;
	const int ROBOT_LOWER_FRAMES = 128; ///< 30 death frames + 98 legs frames
	const int ROBOT_UPPER_FRAMES = 65;	///< 30 death frames + 35 torso frames

	/**
	 * Sequence and progress of a frame of lower.md3 (legs = true) or upper.md3
	 */
	const PlayerSequence &robotSequence(int frame, bool legs, float &t, float &cycle)
	{
		for (const auto &sequence : robotSequences())
		{
			const std::string name = sequence.name;
			const bool both = name.compare(0, 5, "BOTH_") == 0;
			const bool isLegs = name.compare(0, 5, "LEGS_") == 0;
			if (!both && isLegs != legs)
			{
				continue;
			}
			const int first = isLegs ? sequence.first - ROBOT_TORSO_FRAMES : sequence.first;
			if (frame >= first && frame < first + sequence.frames && sequence.frames > 1)
			{
				t = float(frame - first) / float(sequence.frames - 1);
				cycle = 2 * PI * float(frame - first) / float(sequence.frames);
				return sequence;
			}
		}
		// Single frame sequences (DEAD, STAND)
		for (const auto &sequence : robotSequences())
		{
			const std::string name = sequence.name;
			const int first = name.compare(0, 5, "LEGS_") == 0 ? sequence.first - ROBOT_TORSO_FRAMES : sequence.first;
			if (frame == first && sequence.frames == 1)
			{
				t = 1;
				cycle = 0;
				return sequence;
			}
		}
		throw mt_ex("No sequence for frame " + std::to_string(frame));
	}

	/*! How far a death sequence has fallen (the DEAD frames are fully down) */
	float deathFall(const std::string &name, float t)
	{
		return name.find("DEAD") != std::string::npos ? 1.0f : std::min(t * 1.4f, 1.0f);
	}

	/*! Death direction of a BOTH_ sequence */
	float deathDirection(const std::string &name)
	{
		return name.back() == '2' ? -1.0f : 1.0f;
	}

	/**
	 * Rotation around a joint: the shapes are at their places in the model, the joint turns them around its point
	 */
	glm::mat4 turnAround(const glm::vec3 &joint, const glm::quat &turn)
	{
		return glm::translate(joint) * glm::toMat4(turn) * glm::translate(-joint);
	}

	void generateRobot(const std::filesystem::path &folder)
	{
		const std::string path = "models/players/robot/";
		const glm::vec3 pitch{0, 1, 0}, roll{1, 0, 0};

		// Lower: parts 0 = hips, 1 / 2 = left / right thigh, 3 / 4 = left / right shin
		std::vector<std::vector<glm::mat4>> lowerFrames;
		std::vector<glm::vec3> torsoMount;
		for (int frame = 0; frame < ROBOT_LOWER_FRAMES; frame++)
		{
			float t = 0, cycle = 0;
			const std::string name = robotSequence(frame, true, t, cycle).name;
			float left = 0, right = 0, knees = 0, crouch = 0, bob = 0, hipsPitch = 0;
			glm::vec3 shift{0};
			if (name.compare(0, 5, "BOTH_") == 0)
			{
				const float fall = deathFall(name, t);
				hipsPitch = -80 * fall * deathDirection(name);
				shift = {-8 * fall * deathDirection(name), 0, -18 * fall};
				left = 20 * fall;
				right = -10 * fall;
			}
			else if (name == "LEGS_WALK" || name == "LEGS_RUN" || name == "LEGS_BACK" || name == "LEGS_WALKCR")
			{
				const float amount = name == "LEGS_RUN" ? 40.0f : 25.0f;
				const float direction = name == "LEGS_BACK" ? -1.0f : 1.0f;
				left = -amount * std::sin(cycle) * direction;
				right = amount * std::sin(cycle) * direction;
				knees = 30;
				bob = -std::abs(std::sin(cycle)) * (name == "LEGS_RUN" ? 1.5f : 0.6f);
				crouch = name == "LEGS_WALKCR" ? 1.0f : 0.0f;
			}
			else if (name == "LEGS_SWIM")
			{
				left = 15 * std::sin(cycle);
				right = -15 * std::sin(cycle);
				hipsPitch = -60;
			}
			else if (name == "LEGS_JUMP" || name == "LEGS_JUMPB")
			{
				left = right = -30 * std::sin(t * PI);
				knees = 60 * std::sin(t * PI);
				shift = {0, 0, 4 * std::sin(t * PI)};
			}
			else if (name == "LEGS_LAND" || name == "LEGS_LANDB")
			{
				left = right = -20 * (1 - t);
				knees = 40 * (1 - t);
				bob = -3 * (1 - t);
			}
			else if (name == "LEGS_IDLE")
			{
				bob = -0.3f * std::sin(cycle);
			}
			else if (name == "LEGS_IDLECR")
			{
				crouch = 1;
				bob = -0.3f * std::sin(cycle);
			}
			else if (name == "LEGS_TURN")
			{
				left = 10 * std::sin(cycle);
				right = -10 * std::sin(cycle);
			}
			if (crouch > 0)
			{
				left -= 50;
				right -= 50;
				knees += 90;
				bob -= 7;
			}

			const glm::mat4 hips = glm::translate(shift + glm::vec3{0, 0, bob}) * glm::toMat4(rotation(hipsPitch, pitch));
			auto leg = [&](float side, float angle) {
				const glm::mat4 thigh = hips * turnAround({0, 3 * side, -2}, rotation(angle, pitch));
				const glm::mat4 shin = thigh * turnAround({0, 3 * side, -12}, rotation(std::max(knees, 0.0f), pitch));
				return std::make_pair(thigh, shin);
			};
			auto [leftThigh, leftShin] = leg(1, left);
			auto [rightThigh, rightShin] = leg(-1, right);
			lowerFrames.push_back({hips, leftThigh, rightThigh, leftShin, rightShin});
			torsoMount.push_back(glm::vec3(hips * glm::vec4(0, 0, 1.5f, 1)));
		}

		MorphMeshDefinition lowerMesh{"l_legs", "", {}};
		lowerMesh.shapes.push_back({box({0, 0, -0.5f}, {3, 5, 2}), 0});
		for (int side = 0; side < 2; side++)
		{
			const float s = side ? -1.0f : 1.0f;
			lowerMesh.shapes.push_back({tube({0, 3 * s, -2}, {0, 3 * s, -12}, 2.0f, 1.6f, 8), 1 + side});
			lowerMesh.shapes.push_back({box({0, 3 * s, -12}, {1.8f, 1.8f, 1.8f}), 3 + side});
			lowerMesh.shapes.push_back({tube({0, 3 * s, -12}, {0, 3 * s, -21}, 1.6f, 1.4f, 8), 3 + side});
			lowerMesh.shapes.push_back({box({1.2f, 3 * s, -22.5f}, {3, 1.6f, 1}), 3 + side});
		}
		auto lower = buildMorph(path + "lower.md3", {lowerMesh}, {{"tag_torso", 0, {0, 0, 1.5f}, glm::quat{1, 0, 0, 0}}}, lowerFrames);

		// Upper: parts 0 = torso, 1 / 2 = left / right upper arm, 3 / 4 = left / right forearm
		std::vector<std::vector<glm::mat4>> upperFrames;
		for (int frame = 0; frame < ROBOT_UPPER_FRAMES; frame++)
		{
			float t = 0, cycle = 0;
			const std::string name = robotSequence(frame, false, t, cycle).name;
			float bend = 0, leftArm = 0, rightArm = 0, leftLift = 0, rightLift = 0, elbows = 20;
			if (name.compare(0, 5, "BOTH_") == 0)
			{
				const float fall = deathFall(name, t);
				bend = -20 * fall * deathDirection(name);
				leftLift = 60 * fall;
				rightLift = 60 * fall;
			}
			else if (name == "TORSO_GESTURE")
			{
				leftLift = 140 * std::min(t * 3, 1.0f);
				elbows = 20 + 30 * std::sin(cycle * 2);
			}
			else if (name == "TORSO_ATTACK" || name == "TORSO_ATTACK2")
			{
				rightArm = -80 + (name == "TORSO_ATTACK" ? 10 * std::sin(t * PI) : 60 * std::sin(t * PI));
				elbows = 10;
			}
			else if (name == "TORSO_DROP")
			{
				rightArm = -80 * (1 - t);
			}
			else if (name == "TORSO_RAISE")
			{
				rightArm = -80 * t;
			}
			else if (name == "TORSO_STAND")
			{
				rightArm = -80;
			}
			else if (name == "TORSO_STAND2")
			{
				rightArm = -40;
				leftArm = -30;
			}

			const glm::mat4 torso = glm::toMat4(rotation(bend, pitch));
			auto arm = [&](float side, float swing, float lift) {
				const glm::mat4 upperArm = torso * turnAround({0, 7 * side, 14}, rotation(swing, pitch) * rotation(lift * side, -roll));
				const glm::mat4 forearm = upperArm * turnAround({0, 7 * side, 6}, rotation(-elbows, pitch));
				return std::make_pair(upperArm, forearm);
			};
			auto [leftUpper, leftFore] = arm(1, leftArm, leftLift);
			auto [rightUpper, rightFore] = arm(-1, rightArm, rightLift);
			upperFrames.push_back({torso, leftUpper, rightUpper, leftFore, rightFore});
		}

		MorphMeshDefinition torsoMesh{"u_torso", "", {}};
		torsoMesh.shapes.push_back({box({0, 0, 3}, {2.8f, 4.5f, 2.5f}), 0});
		torsoMesh.shapes.push_back({box({0, 0, 10.5f}, {3.5f, 6, 5}), 0});
		torsoMesh.shapes.push_back({tube({0, 0, 15.5f}, {0, 0, 18}, 1.5f, 1.5f, 8), 0});
		MorphMeshDefinition leftArmMesh{"u_larm", "", {}}, rightArmMesh{"u_rarm", "", {}};
		for (int side = 0; side < 2; side++)
		{
			auto &mesh = side ? rightArmMesh : leftArmMesh;
			const float s = side ? -1.0f : 1.0f;
			mesh.shapes.push_back({box({0, 7 * s, 14}, {1.8f, 1.8f, 1.8f}), 1 + side});
			mesh.shapes.push_back({tube({0, 7 * s, 14}, {0, 7 * s, 6}, 1.4f, 1.2f, 8), 1 + side});
			mesh.shapes.push_back({tube({0, 7 * s, 6}, {0, 7 * s, -1}, 1.2f, 1.0f, 8), 3 + side});
			mesh.shapes.push_back({box({0, 7 * s, -2}, {1.2f, 0.7f, 1.2f}), 3 + side});
		}
		auto upper = buildMorph(path + "upper.md3", {torsoMesh, leftArmMesh, rightArmMesh},
								{{"tag_head", 0, {0, 0, 18}, glm::quat{1, 0, 0, 0}},
								 {"tag_weapon", 4, {1, -7, -2.5f}, rotation(-90, pitch)},
								 {"tag_torso", 0, {0, 0, 0}, glm::quat{1, 0, 0, 0}}},
								upperFrames);

		// Head with two levels of detail
		auto head = [&](int rings, int sides, bool antenna) {
			MorphMeshDefinition mesh{"h_head", "", {}};
			mesh.shapes.push_back({sphere({0, 0, 4}, 4.5f, rings, sides), 0});
			mesh.shapes.push_back({box({3.5f, 0, 4.5f}, {1.2f, 3, 1}), 0});
			if (antenna)
			{
				mesh.shapes.push_back({tube({0, 0, 8}, {-1, 1, 12}, 0.4f, 0.2f, 5), 0});
			}
			return buildMorph(path + "head.md3", {mesh}, {{"tag_head", 0, {0, 0, 0}, glm::quat{1, 0, 0, 0}}}, {{glm::mat4{1.0f}}});
		};
		auto headModel = head(8, 12, true);
		headModel->lods.push_back(head(6, 8, true)->meshes);
		headModel->lods.push_back(head(4, 6, false)->meshes);

		writeMD3(*lower, (folder / "lower.md3").string());
		writeMD3(*upper, (folder / "upper.md3").string());
		writeMD3(*headModel, (folder / "head.md3").string());

		// animation.cfg
		std::string config = "// animation config file (generated by "
							 "tools/fixtures/generate_fixtures.cpp)\n\nsex\tn\nfootsteps\tmech\nheadoffset\t-2 0 0\n\n"
							 "// first frame, num frames, looping frames, frames per second\n\n";
		for (const auto &sequence : robotSequences())
		{
			config += std::to_string(sequence.first) + "\t" + std::to_string(sequence.frames) + "\t" + std::to_string(sequence.loop) + "\t"
					  + std::to_string(sequence.fps) + "\t\t// " + sequence.name + "\n";
		}
		writeText(folder / "animation.cfg", config);

		// Skins and their textures
		const std::vector<std::pair<std::string, glm::vec3>> skins = {{"default", {0.62f, 0.64f, 0.68f}},
																	  {"red", {0.75f, 0.2f, 0.18f}},
																	  {"blue", {0.2f, 0.35f, 0.8f}},
																	  {"stripe", {0.85f, 0.75f, 0.2f}}};
		for (const auto &[skin, color] : skins)
		{
			const std::string texture = skin == "default" ? "robot" : skin;
			const std::string body = path + texture + ".tga", face = path + texture + "_h.tga";
			writeText(folder / ("lower_" + skin + ".skin"), "tag_torso,\nl_legs," + body + "\n");
			writeText(folder / ("upper_" + skin + ".skin"),
					  "tag_head,\ntag_weapon,\nu_torso," + body + "\ntag_torso,\nu_larm," + body + "\nu_rarm," + body + "\n");
			writeText(folder / ("head_" + skin + ".skin"), "tag_head,\nh_head," + face + "\n");

			const bool striped = skin == "stripe";
			writeTga(folder / (texture + ".tga"), 64, [color = color, striped](float u, float v) {
				// Panels with dark seams and rivets
				const float seam = (std::fmod(u * 4, 1.0f) < 0.06f || std::fmod(v * 4, 1.0f) < 0.06f) ? 0.55f : 1.0f;
				const float rivet = glm::length(glm::vec2{std::fmod(u * 4, 1.0f), std::fmod(v * 4, 1.0f)} - 0.2f) < 0.06f ? 0.7f : 1.0f;
				const float stripe = striped && std::fmod(u * 8 + v * 8, 2.0f) < 1.0f ? 0.6f : 1.0f;
				return color * seam * rivet * stripe * (0.9f + 0.1f * v);
			});
			writeTga(folder / (texture + "_h.tga"), 64, [color = color](float u, float v) {
				// Visor band on the face texture
				const bool visor = v > 0.45f && v < 0.6f;
				return visor ? glm::vec3{0.1f, 0.9f, 0.8f} * (0.8f + 0.2f * u) : color * (0.8f + 0.2f * v);
			});
		}
	}
}

namespace {

	/*
	==============================================================================
	ET character: the mannequin as the body of two .char files (soldier and medic) with a hud head, accessories,
	skins, an animation group of two MDX files and head animations
	==============================================================================
	*/

	const int HEAD_FRAMES = 30;

	/*! Hud head: frames 0-9 nod, 10-19 turn left, 20-29 turn right. tag_mouth moves with the head */
	std::unique_ptr<morph::MorphModel> buildCharacterHead()
	{
		MorphMeshDefinition mesh{"h_head", "", {}};
		mesh.shapes.push_back({sphere({0, 0, 4}, 4.0f, 8, 12), 0});
		mesh.shapes.push_back({box({3.4f, 0, 4.0f}, {0.8f, 2.6f, 0.7f}), 0}); // visor like face plate
		mesh.shapes.push_back({box({3.6f, 0, 1.6f}, {0.6f, 1.4f, 0.4f}), 0}); // mouth

		std::vector<std::vector<glm::mat4>> frames;
		for (int frame = 0; frame < HEAD_FRAMES; frame++)
		{
			const float t = float(frame % 10) / 9.0f;
			const float wave = std::sin(t * PI);
			glm::quat turn{1, 0, 0, 0};
			if (frame < 10)
			{
				turn = rotation(8 * wave, {0, 1, 0});
			}
			else if (frame < 20)
			{
				turn = rotation(35 * wave, {0, 0, 1});
			}
			else
			{
				turn = rotation(-35 * wave, {0, 0, 1});
			}
			frames.push_back({glm::toMat4(turn)});
		}

		return buildMorph("models/players/mannequin/head.md3", {mesh}, {{"tag_mouth", 0, {3.0f, 0, 2.0f}, glm::quat{1, 0, 0, 0}}}, frames);
	}

	/*! Single frame accessory, the shapes are relative to the tag it goes on */
	std::unique_ptr<morph::MorphModel>
	buildAccessory(const std::string &path, const std::string &surface, const std::string &material, const std::vector<Shape> &shapes)
	{
		MorphMeshDefinition mesh{surface, material, {}};
		for (const auto &shape : shapes)
		{
			mesh.shapes.push_back({shape, 0});
		}
		return buildMorph(path, {mesh}, {}, {{glm::mat4{1.0f}}});
	}

	void generateCharacter(const std::filesystem::path &data)
	{
		const std::string folder = "models/players/mannequin/";
		std::filesystem::create_directories(data / folder);
		std::filesystem::create_directories(data / "characters/mannequin");
		std::filesystem::create_directories(data / "animations/scripts");

		writeMD3(*buildCharacterHead(), (data / folder / "head.md3").string());
		// Relative to the head's tag_mouth (3 forward, 2 up from the head origin)
		writeMD3(*buildAccessory(folder + "helmet.md3", "helmet", folder + "helmet.tga",
								 {sphere({-3, 0, 4.2f}, 4.4f, 6, 12), box({-3, 0, 3.8f}, {4.9f, 4.7f, 0.25f})}),
				 (data / folder / "helmet.md3").string());
		writeMD3(*buildAccessory(folder + "rank.md3", "rank", "", {box({-3, -4.4f, 5}, {0.8f, 0.2f, 0.8f})}),
				 (data / folder / "rank.md3").string());
		// Relative to tag_back (turned around: +X points away from the back)
		writeMD3(*buildAccessory(folder + "backpack.md3", "backpack", folder + "gear.tga",
								 {box({2.0f, 0, -1}, {2.0f, 3.6f, 4.2f}), box({2.0f, 0, 3.6f}, {1.6f, 3.0f, 0.8f})}),
				 (data / folder / "backpack.md3").string());
		writeMD3(*buildAccessory(folder + "pouch.md3", "pouch", folder + "gear.tga", {box({1.5f, -1.0f, -1}, {1.2f, 0.9f, 1.4f})}),
				 (data / folder / "pouch.md3").string());

		// Textures
		auto plain = [&](const std::string &name, glm::vec3 color, glm::vec3 stripe) {
			writeTga(data / folder / (name + ".tga"), 32,
					 [=](float u, float v) { return std::fmod(std::floor(u * 8) + std::floor(v * 8), 2.0f) < 1.0f ? color : stripe; });
		};
		plain("soldier", {0.35f, 0.40f, 0.30f}, {0.30f, 0.34f, 0.26f});
		plain("medic", {0.85f, 0.85f, 0.85f}, {0.80f, 0.20f, 0.20f});
		plain("naked", {0.80f, 0.65f, 0.55f}, {0.76f, 0.61f, 0.52f});
		plain("face", {0.85f, 0.70f, 0.60f}, {0.80f, 0.66f, 0.56f});
		plain("helmet", {0.25f, 0.30f, 0.22f}, {0.22f, 0.27f, 0.20f});
		plain("helmet_medic", {0.90f, 0.90f, 0.90f}, {0.85f, 0.15f, 0.15f});
		plain("gear", {0.40f, 0.30f, 0.20f}, {0.36f, 0.27f, 0.18f});

		const std::string header = "// Test character, generated by tools/fixtures/generate_fixtures.cpp\n";
		auto body = [&](const std::string &texture) {
			const std::string path = folder + texture + ".tga";
			return "l_legs,\t\"" + path + "\"\nu_body,\t\"" + path + "\"\nu_rthand,\t\"" + path + "\"\nu_lfthand,\t\"" + path + "\"\n";
		};

		// Body skins: models/mdm/mannequin_<skin>.skin, with the accessories
		writeText(data / "models/mdm/mannequin_soldier.skin", header + "md3_hat,\t\"" + folder + "helmet.md3\"\nmd3_rank,\t\"" + folder
																  + "rank.md3\"\nmd3_back,\t\"" + folder + "backpack.md3\"\nmd3_beltr,\t\""
																  + folder + "pouch.md3\"\n//md3_belt,\t\"" + folder + "pouch.md3\"\n\n"
																  + body("soldier"));
		writeText(data / "models/mdm/mannequin_medic.skin",
				  header + "md3_hat,\t\"" + folder + "helmet.md3\"\nmd3_back,\t\"" + folder + "backpack.md3\"\n\n" + body("medic"));
		writeText(data / "models/mdm/mannequin_naked.skin", header + body("naked"));

		// Head skins and the optional accessory skin (<model>_<skin>.skin)
		writeText(data / folder / "head_soldier.skin", "h_head, \"" + folder + "face\"\n");
		writeText(data / folder / "head_medic.skin", "h_head, \"" + folder + "face\"\n");
		writeText(data / folder / "helmet_medic.skin", "helmet, \"" + folder + "helmet_medic\"\n");

		// Animation group: the mannequin's own MDX and sequences, then a second MDX with inline sequences
		writeText(data / "animations/mannequin.anim",
				  header
					  + "animgroup\n{\n"
						"\tanimfile \"models/mdm/mannequin.mdx\"\n\t{\n"
						"\t\t#include \"models/mdm/mannequin.aninc\"\n\t}\n"
						"\tanimfile \"animations/mannequin/salute.mdx\"\n\t{\n"
						"\t\t// <animation name>\t<first frame> <length> <looping> <fps> <move speed> <transition> <reversed>\n"
						"\t\tsalute\t\t0\t20\t0\t20\t0\t100\t0\n"
						"\t\tsalute_hold\t19\t1\t1\t20\t0\t0\t0\n"
						"\t}\n}\n");
		// The movement script is listed in the .char but not read by ModelTool
		writeText(data / "animations/scripts/mannequin.script", header + "// Movement states would go here (not read by ModelTool)\n");

		// Head animations: first, length, fps, looping frames for HD_IDLE1 ... HD_PAIN (16 lines)
		std::string head = header + "// first\tlength\tfps\tlooping\n";
		const std::array<std::array<int, 4>, 16> headLines = {{
			{0, 10, 10, 10}, // HD_IDLE1
			{0, 5, 10, 0},	 // HD_IDLE2
			{10, 10, 10, 0}, // HD_IDLE3
			{0, 30, 10, 30}, // HD_IDLE4
			{0, 10, 5, 0},	 // HD_IDLE5
			{10, 20, 10, 0}, // HD_IDLE6
			{20, 10, 5, 0},	 // HD_IDLE7
			{4, 2, 10, 0},	 // HD_IDLE8
			{0, 10, 10, 10}, // HD_DAMAGED_IDLE1
			{10, 10, 10, 0}, // HD_DAMAGED_IDLE2
			{20, 10, 10, 0}, // HD_DAMAGED_IDLE3
			{10, 10, 10, 0}, // HD_LEFT
			{20, 10, 10, 0}, // HD_RIGHT
			{0, 5, 30, 2},	 // HD_ATTACK
			{5, 5, 20, 0},	 // HD_ATTACK_END
			{0, 10, 15, 0},	 // HD_PAIN
		}};
		for (const auto &line : headLines)
		{
			head += std::to_string(line[0]) + "\t" + std::to_string(line[1]) + "\t" + std::to_string(line[2]) + "\t"
					+ std::to_string(line[3]) + "\n";
		}
		writeText(data / "animations/mannequin/head.anim", head);

		auto character = [&](const std::string &skin) {
			return header + "characterDef\n{\n\tskin\t\t\t\"" + skin
				   + "\"\n\n"
					 "\tmesh\t\t\t\"models/mdm/mannequin.mdm\"\n"
					 "\tanimationgroup\t\t\"animations/mannequin.anim\"\n"
					 "\tanimationscript\t\t\"animations/scripts/mannequin.script\"\n\n"
					 "\tundressedCorpseModel\t\"models/mdm/mannequin.mdm\"\n"
					 "\tundressedCorpseSkin\t\"naked\"\n\n"
					 "\thudhead\t\t\t\""
				   + folder + "head.md3\"\n\thudheadanims\t\t\"animations/mannequin/head.anim\"\n\thudheadskin\t\t\"" + folder + "head_"
				   + skin + ".skin\"\n}\n";
		};
		writeText(data / "characters/mannequin/soldier.char", character("soldier"));
		writeText(data / "characters/mannequin/medic.char", character("medic"));
	}

	/*
	==============================================================================
	Shader script
	==============================================================================
	*/

	void generateShaders(const std::filesystem::path &data)
	{
		// Sky box sides in the game's order (rt, bk, lf, ft, up, dn): one color each, darker towards the bottom edge of
		// the image, so the orientation can be seen
		const std::pair<const char *, glm::vec3> sides[]
			= {{"rt", {0.8f, 0.2f, 0.2f}}, {"bk", {0.2f, 0.7f, 0.2f}}, {"lf", {0.2f, 0.3f, 0.9f}},
			   {"ft", {0.9f, 0.8f, 0.2f}}, {"up", {0.6f, 0.8f, 1.0f}}, {"dn", {0.3f, 0.25f, 0.2f}}};
		for (const auto &[suffix, color] : sides)
		{
			writeTga(data / (std::string("models/players/mannequin/sky_") + suffix + ".tga"), 32, [color = color](float u, float v) {
				return color * (0.5f + 0.5f * v) + glm::vec3(u > 0.45f && u < 0.55f ? 0.2f : 0.0f);
			});
		}

		// Quake 3 / ET shaders for the mannequin textures: cull modes, implicit maps, alpha test, blending, animMap
		// after a lightmap stage, the editor image and the comment styles
		writeText(data / "scripts/mannequin.shader", R"(// Generated by tools/fixtures/generate_fixtures.cpp

models/players/mannequin/gear
{
	cull none
	{
		map models/players/mannequin/gear.tga
		rgbGen lightingDiffuse
	}
}

models/players/mannequin/strap
{
	cull disable
	implicitMask models/players/mannequin/gear.tga
}

/* A lightmapped shader: the image stage is drawn opaque on its own */
models/players/mannequin/glass
{
	qer_editorimage models/players/mannequin/helmet.tga
	surfaceparm trans
	{
		map $lightmap
	}
	{
		animMap 4 models/players/mannequin/medic.tga models/players/mannequin/soldier.tga
		blendFunc GL_DST_COLOR GL_ZERO
	}
}

models/players/mannequin/decal
{
	cull back // comment after a keyword
	nopicmip
	surfaceparm nonsolid
	surfaceparm TRANS
	{
		clampMap models/players/mannequin/face.tga
		blendFunc add
		alphaFunc GT0
		tcGen environment
	}
}

textures/mannequin/sky
{
	surfaceparm sky
	skyParms models/players/mannequin/sky 512 -
}

textures/mannequin/sky_no_box
{
	skyParms - 512 -
}

models/players/mannequin/editor_only
{
	qer_editorimage models/players/mannequin/naked.tga
	surfaceparm nodraw
}

models/players/mannequin/gear
{
	// A second definition in the same file does not count
	cull back
}
)");
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: generate_fixtures <data folder>\n";
		return 2;
	}

	try
	{
		const std::filesystem::path data(argv[1]);
		for (const char *folder : {"models/fbx", "models/md3", "models/mdc", "models/mdm", "models/ms3d", "models/players/robot"})
		{
			std::filesystem::create_directories(data / folder);
		}
		generateTube(data);
		generateMannequin(data / "models/mdm");
		generateMs3d(data / "models/ms3d/test.ms3d");
		writeText(data / "models/fbx/cube.fbx", fbxFile("Cube", fbxCube(), 0));
		writeText(data / "models/fbx/cone.fbx", fbxFile("Cone", fbxCone(16), 24));
		generateRobot(data / "models/players/robot");
		generateCharacter(data);
		generateShaders(data);
		std::cout << "Generated the fixtures in " << data.string() << "\n";
		return 0;
	}
	catch (const std::exception &ex)
	{
		std::cerr << "Error: " << ex.what() << "\n";
		return 1;
	}
}
