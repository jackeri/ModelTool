#include "gtest/gtest.h"
#include "model_ufbx.h"
#include "model_gltf.h"
#include "measure.h"
#include "scene_import.h"
#include "skeletal_model.h"
#include "posed_model.h"
#include "skeletal_tools.h"
#include "morph_model.h"
#include "normals.h"
#include "mt_path.h"
#include "filesystem.h"

#include <cstring>
#include <filesystem>
#include <fstream>

using namespace mt;
using namespace mt::model;

// glTF is in meters, the import gives game units (1 unit = 1 inch)
constexpr float UNITS_PER_METER = 1.0f / METERS_PER_UNIT;

namespace {
	Ref<mt::io::MTFile> memoryFile(const std::string &name, const std::vector<byte> &data)
	{
		auto file = make_ref<mt::io::MTFile>();
		file->data = make_ref<std::vector<byte>>(data);
		file->len = data.size();
		file->data->push_back(0);
		file->name = name;
		file->ext = model::io::normalizeExtension(name);
		return file;
	}

	Ref<mt::io::MTFile> textFile(const std::string &name, const std::string &text)
	{
		return memoryFile(name, std::vector<byte>(text.begin(), text.end()));
	}

	template<typename T>
	std::unique_ptr<T> load(const Ref<mt::io::MTFile> &file)
	{
		std::unique_ptr<Model> loaded(model::io::loadModel(file));
		auto *typed = dynamic_cast<T *>(loaded.get());
		if (typed)
		{
			loaded.release();
		}
		return std::unique_ptr<T>(typed);
	}

	void expectVec(const glm::vec3 &actual, const glm::vec3 &expected, float tolerance = 1e-4f)
	{
		EXPECT_NEAR(actual.x, expected.x, tolerance);
		EXPECT_NEAR(actual.y, expected.y, tolerance);
		EXPECT_NEAR(actual.z, expected.z, tolerance);
	}

	Bounds bindBounds(const skeletal::SkeletalModel &model)
	{
		Bounds bounds{glm::vec3(1e9f), glm::vec3(-1e9f)};
		for (const auto &mesh : model.meshes)
		{
			for (const auto &vert : mesh.vertices)
			{
				bounds.min = glm::min(bounds.min, vert.pos);
				bounds.max = glm::max(bounds.max, vert.pos);
			}
		}
		return bounds;
	}

	std::string base64(const std::vector<byte> &data)
	{
		static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		for (size_t i = 0; i < data.size(); i += 3)
		{
			uint32_t value = uint32_t(data[i]) << 16u;
			if (i + 1 < data.size())
			{
				value |= uint32_t(data[i + 1]) << 8u;
			}
			if (i + 2 < data.size())
			{
				value |= uint32_t(data[i + 2]);
			}
			out += chars[(value >> 18u) & 63u];
			out += chars[(value >> 12u) & 63u];
			out += i + 1 < data.size() ? chars[(value >> 6u) & 63u] : '=';
			out += i + 2 < data.size() ? chars[value & 63u] : '=';
		}
		return out;
	}

	/*! Builds the buffer, buffer views and accessors of a small glTF file */
	class GltfBuffers {
	  public:
		int floats(const std::vector<float> &values, const char *type, int components)
		{
			return add(values.data(), values.size() * sizeof(float), 5126, values.size() / size_t(components), type);
		}

		int indices(const std::vector<uint16_t> &values)
		{
			return add(values.data(), values.size() * sizeof(uint16_t), 5123, values.size(), "SCALAR");
		}

		int bytes(const std::vector<uint8_t> &values, const char *type, int components)
		{
			return add(values.data(), values.size(), 5121, values.size() / size_t(components), type);
		}

		/**
		 * @param embed true: data URI buffer (.gltf), false: GLB binary chunk buffer
		 */
		[[nodiscard]] std::string json(bool embed = true) const
		{
			std::string buffer = "{\"byteLength\": " + std::to_string(bin.size());
			if (embed)
			{
				buffer += ", \"uri\": \"data:application/octet-stream;base64," + base64(bin) + "\"";
			}
			buffer += "}";
			return "\"buffers\": [" + buffer + "], \"bufferViews\": [" + views + "], \"accessors\": [" + accessors + "]";
		}

		std::vector<byte> bin;

	  private:
		int add(const void *data, size_t size, int componentType, size_t count, const char *type)
		{
			while (bin.size() % 4)
			{
				bin.push_back(0);
			}

			const size_t offset = bin.size();
			const auto *bytes = static_cast<const byte *>(data);
			bin.insert(bin.end(), bytes, bytes + size);

			const int index = numAccessors++;
			views += std::string(index ? ", " : "") + "{\"buffer\": 0, \"byteOffset\": " + std::to_string(offset)
					 + ", \"byteLength\": " + std::to_string(size) + "}";
			accessors += std::string(index ? ", " : "") + "{\"bufferView\": " + std::to_string(index) + ", \"componentType\": "
						 + std::to_string(componentType) + ", \"count\": " + std::to_string(count) + ", \"type\": \"" + type + "\"}";
			return index;
		}

		std::string views, accessors;
		int numAccessors = 0;
	};

	/*! Triangle on a node that moves 2 units along x in one second, with a tag node */
	std::string staticGltf(bool embed = true, GltfBuffers *out = nullptr)
	{
		GltfBuffers buffers;
		int positions = buffers.floats({0, 0, 0, 1, 0, 0, 0, 1, 0}, "VEC3", 3);
		int normals = buffers.floats({0, 0, 1, 0, 0, 1, 0, 0, 1}, "VEC3", 3);
		int uvs = buffers.floats({0, 0, 1, 0, 0, 1}, "VEC2", 2);
		int indices = buffers.indices({0, 1, 2});
		int times = buffers.floats({0, 1}, "SCALAR", 1);
		int moves = buffers.floats({0, 0, 0, 2, 0, 0}, "VEC3", 3);

		std::string json
			= "{\"asset\": {\"version\": \"2.0\"}, \"scene\": 0, \"scenes\": [{\"nodes\": [0]}],"
			  "\"nodes\": [{\"name\": \"triangle\", \"mesh\": 0, \"children\": [1]}, {\"name\": \"tag_flash\", \"translation\": [0, 1, "
			  "0]}],"
			  "\"materials\": [{\"name\": \"metal\"}],"
			  "\"meshes\": [{\"name\": \"tri\", \"primitives\": [{\"attributes\": {\"POSITION\": "
			  + std::to_string(positions) + ", \"NORMAL\": " + std::to_string(normals) + ", \"TEXCOORD_0\": " + std::to_string(uvs)
			  + "}, \"indices\": " + std::to_string(indices) + ", \"material\": 0}]}],"
			  + "\"animations\": [{\"name\": \"move\", \"samplers\": [{\"input\": " + std::to_string(times)
			  + ", \"output\": " + std::to_string(moves)
			  + "}], \"channels\": [{\"sampler\": 0, \"target\": {\"node\": 0, \"path\": \"translation\"}}]}]," + buffers.json(embed) + "}";

		if (out)
		{
			*out = buffers;
		}
		return json;
	}

	/*! Two bones, the second one turns 90 degrees around y in one second */
	std::string skinnedGltf()
	{
		GltfBuffers buffers;
		int positions = buffers.floats({0, 0, 0, 2, 0, 0, 1, 1, 0}, "VEC3", 3);
		int joints = buffers.bytes({0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}, "VEC4", 4);
		int weights = buffers.floats({1, 0, 0, 0, 1, 0, 0, 0, 0.5f, 0.5f, 0, 0}, "VEC4", 4);
		int indices = buffers.indices({0, 1, 2});
		// Column major inverse bind matrices: identity and translate(-1, 0, 0)
		int inverseBind
			= buffers.floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1}, "MAT4", 16);
		int times = buffers.floats({0, 1}, "SCALAR", 1);
		int rotations = buffers.floats({0, 0, 0, 1, 0, 0.70710678f, 0, 0.70710678f}, "VEC4", 4);

		return "{\"asset\": {\"version\": \"2.0\"}, \"scene\": 0, \"scenes\": [{\"nodes\": [0, 2]}],"
			   "\"nodes\": [{\"name\": \"root\", \"children\": [1]}, {\"name\": \"arm\", \"translation\": [1, 0, 0]}, {\"name\": \"body\", "
			   "\"mesh\": 0, \"skin\": 0}],"
			   "\"skins\": [{\"joints\": [0, 1], \"inverseBindMatrices\": "
			   + std::to_string(inverseBind) + "}]," + "\"meshes\": [{\"name\": \"body\", \"primitives\": [{\"attributes\": {\"POSITION\": "
			   + std::to_string(positions) + ", \"JOINTS_0\": " + std::to_string(joints) + ", \"WEIGHTS_0\": " + std::to_string(weights)
			   + "}, \"indices\": " + std::to_string(indices) + "}]}],"
			   + "\"animations\": [{\"name\": \"bend\", \"samplers\": [{\"input\": " + std::to_string(times)
			   + ", \"output\": " + std::to_string(rotations)
			   + "}], \"channels\": [{\"sampler\": 0, \"target\": {\"node\": 1, \"path\": \"rotation\"}}]}]," + buffers.json() + "}";
	}

	std::vector<byte> glb(const std::string &json, const std::vector<byte> &bin)
	{
		std::string paddedJson = json;
		while (paddedJson.size() % 4)
		{
			paddedJson += ' ';
		}
		std::vector<byte> paddedBin = bin;
		while (paddedBin.size() % 4)
		{
			paddedBin.push_back(0);
		}

		std::vector<byte> out;
		auto put32 = [&](uint32_t value) {
			for (int i = 0; i < 4; i++)
			{
				out.push_back(byte((value >> (8u * unsigned(i))) & 0xffu));
			}
		};

		out.insert(out.end(), {'g', 'l', 'T', 'F'});
		put32(2);
		put32(uint32_t(12 + 8 + paddedJson.size() + 8 + paddedBin.size()));
		put32(uint32_t(paddedJson.size()));
		put32(0x4E4F534Au); // JSON
		out.insert(out.end(), paddedJson.begin(), paddedJson.end());
		put32(uint32_t(paddedBin.size()));
		put32(0x004E4942u); // BIN
		out.insert(out.end(), paddedBin.begin(), paddedBin.end());
		return out;
	}
}

TEST(ImportFBXTest, staticCube)
{
	mt::io::MTPath data("data");
	auto model = load<morph::MorphModel>(data.loadFile("models/fbx/cube.fbx"));
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "cube");
	ASSERT_EQ(model->numFrames(), 1);
	ASSERT_EQ(model->meshes.size(), 1);
	ASSERT_EQ(model->meshes[0].name, "Cube");
	ASSERT_EQ(model->meshes[0].numVertices(), 24);
	ASSERT_EQ(model->meshes[0].tris.size(), 12);

	// The coordinates are kept as they are: the cube is 2 units wide
	expectVec(model->frames[0].bounds.min, {-1, -1, -1});
	expectVec(model->frames[0].bounds.max, {1, 1, 1});

	// The triangles are turned to the idTech winding: calculated normals match the file normals
	std::vector<glm::vec3> stored;
	for (const auto &vert : model->meshes[0].frames[0])
	{
		stored.push_back(vert.normal);
	}
	calculateNormals(*model);
	for (size_t v = 0; v < stored.size(); v++)
	{
		ASSERT_GT(glm::dot(stored[v], model->meshes[0].frames[0][v].normal), 0.99f);
	}
}

TEST(ImportFBXTest, animationThatMovesNothing)
{
	// The cone has a 24 frame take that does not move the mesh, it is loaded as a single frame
	mt::io::MTPath data("data");
	auto model = load<morph::MorphModel>(data.loadFile("models/fbx/cone.fbx"));
	ASSERT_TRUE(model);
	ASSERT_EQ(model->numFrames(), 1);
	ASSERT_TRUE(model->animations.empty());
	ASSERT_NO_THROW(model->validate());
}

TEST(ImportFBXTest, skinnedBindPoseLikeFrames)
{
	// The checks of the glTF bind pose fix for FBX: the Y-up file converted by ufbx keeps the bind pose, the bones and the
	// animation in one space (vertices from the bones and weights = the stored bind vertices), and the bind pose is the
	// IQM version of the model exactly (by nearest vertex, the vertex order differs)
	mt::io::MTPath data("data");
	auto fbx = load<skeletal::SkeletalModel>(data.loadFile("models/fbx/mrfixit.fbx"));
	auto iqm = load<skeletal::SkeletalModel>(data.loadFile("models/iqm/mrfixit.iqm"));
	ASSERT_TRUE(fbx && iqm);

	for (const auto &mesh : fbx->meshes)
	{
		for (const auto &vert : mesh.vertices)
		{
			ASSERT_LT(glm::distance(vert.pos, skeletal::posedVertex(*fbx, mesh, vert, FRAME_BIND_POSE)), 1e-4f);
		}
	}

	PoseOptions bind;
	bind.bindPose = true;
	const auto a = poseModel(*fbx, bind), b = poseModel(*iqm, bind);
	for (const auto &meshB : b.meshes)
	{
		auto meshA = std::find_if(a.meshes.begin(), a.meshes.end(), [&](const PosedMesh &m) { return m.name == meshB.name; });
		ASSERT_NE(meshA, a.meshes.end());
		for (size_t v = 0; v < meshB.positions.size(); v += 7)
		{
			float nearest = std::numeric_limits<float>::max();
			for (const auto &p : meshA->positions)
			{
				nearest = std::min(nearest, glm::distance(p, meshB.positions[v]));
			}
			ASSERT_LT(nearest, 1e-3f) << meshB.name << " vertex " << v;
		}
	}

	// The bones are at the IQM bones' places (their axes differ by a constant 90 degrees around their own Z, a bone
	// axis convention of the two exporters, not the Y-up conversion, which turns around X)
	for (const auto &bone : fbx->joints)
	{
		for (const auto &other : iqm->joints)
		{
			if (bone.name == other.name)
			{
				EXPECT_LT(glm::distance(bone.location, other.location), 1e-3f) << bone.name;
				const glm::quat offset = glm::inverse(glm::normalize(other.rotation)) * glm::normalize(bone.rotation);
				EXPECT_NEAR(std::abs(glm::dot(glm::axis(offset), glm::vec3(0, 0, 1))), 1.0f, 1e-3f) << bone.name;
			}
		}
	}
}

TEST(ImportFBXTest, skinnedMatchesIQM)
{
	mt::io::MTPath data("data");
	auto fbx = load<skeletal::SkeletalModel>(data.loadFile("models/fbx/mrfixit.fbx"));
	auto iqm = load<skeletal::SkeletalModel>(data.loadFile("models/iqm/mrfixit.iqm"));
	ASSERT_TRUE(fbx && iqm);

	ASSERT_TRUE(fbx->hasBindPose);
	ASSERT_EQ(fbx->currentFrame, FRAME_BIND_POSE);
	ASSERT_EQ(fbx->joints.size(), 76); // the IQM bones and the armature
	ASSERT_EQ(fbx->numFrames(), 352);
	ASSERT_EQ(fbx->bounds.size(), 352);
	ASSERT_EQ(fbx->animations.size(), 2);
	ASSERT_EQ(fbx->animations[1].name, "idle");
	ASSERT_EQ(fbx->animations[1].startFrame, 251);
	ASSERT_EQ(fbx->animations[1].numFrames, 101);
	ASSERT_EQ(fbx->animations[1].fps, 24);

	// Same meshes, same size and orientation as the IQM version of the model
	for (const auto &mesh : iqm->meshes)
	{
		auto found = std::find_if(fbx->meshes.begin(), fbx->meshes.end(), [&](const skeletal::Mesh &m) { return m.name == mesh.name; });
		ASSERT_NE(found, fbx->meshes.end()) << mesh.name;
		ASSERT_EQ(found->vertices.size(), mesh.vertices.size());
		ASSERT_EQ(found->tris.size(), mesh.tris.size());
	}
	ASSERT_EQ(fbx->meshes[0].materialName, "textures/Head.tga.png");

	Bounds a = bindBounds(*fbx), b = bindBounds(*iqm);
	expectVec(a.min, b.min, 0.05f);
	expectVec(a.max, b.max, 0.05f);

	// The weights agree on the animated vertex positions
	double spread = 0;
	int count = 0;
	for (const auto &mesh : fbx->meshes)
	{
		for (const auto &vert : mesh.vertices)
		{
			glm::vec3 pos = skeletal::posedVertex(*fbx, mesh, vert, 300);
			for (int i = 0; i < vert.weightCount; i++)
			{
				const auto &weight = mesh.weights[vert.startWeight + i];
				const auto &joint = fbx->joints[weight.jointId].frames[300];
				spread += glm::distance(joint.location + joint.rotation * weight.pos, pos);
				count++;
			}
		}
	}
	ASSERT_LT(spread / count, 0.05);
}

TEST(ImportFBXTest, errors)
{
	ASSERT_THROW(loadFBX(textFile("broken.fbx", "this is not an fbx file")), mt_exception);
	ASSERT_THROW(loadFBX(nullptr), mt_exception);
}

TEST(ImportOBJTest, quad)
{
	auto model = load<morph::MorphModel>(textFile("quad.obj", "o quad\n"
															  "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
															  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
															  "vn 0 0 1\n"
															  "usemtl wood\n"
															  "f 1/1/1 2/2/1 3/3/1 4/4/1\n"));
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "quad");
	ASSERT_EQ(model->meshes.size(), 1);
	const auto &mesh = model->meshes[0];
	ASSERT_EQ(mesh.name, "quad");
	ASSERT_EQ(mesh.materialName, "wood");
	ASSERT_EQ(mesh.numVertices(), 4);
	ASSERT_EQ(mesh.tris.size(), 2);

	// OBJ coordinates are used as they are
	expectVec(model->frames[0].bounds.min, {0, 0, 0});
	expectVec(model->frames[0].bounds.max, {1, 1, 0});

	for (size_t v = 0; v < mesh.numVertices(); v++)
	{
		const auto &vert = mesh.frames[0][v];
		expectVec(vert.normal, {0, 0, 1});
		// The texture origin moves from the bottom to the top
		ASSERT_NEAR(mesh.texCoords[v].y, 1.0f - vert.pos.y, 1e-5f);
	}

	auto copy = *model;
	calculateNormals(copy);
	expectVec(copy.meshes[0].frames[0][0].normal, {0, 0, 1});
}

TEST(ImportOBJTest, errors)
{
	ASSERT_THROW(loadOBJ(textFile("empty.obj", "# nothing here\n")), mt_exception);
}

TEST(ImportGLTFTest, staticAnimated)
{
	auto model = load<morph::MorphModel>(textFile("triangle.gltf", staticGltf()));
	ASSERT_TRUE(model);

	ASSERT_EQ(model->name, "triangle");
	ASSERT_EQ(model->numFrames(), 25); // one second at 24 fps
	ASSERT_EQ(model->animations.size(), 1);
	ASSERT_EQ(model->animations[0].name, "move");
	ASSERT_EQ(model->animations[0].numFrames, 25);

	const auto &mesh = model->meshes[0];
	ASSERT_EQ(mesh.name, "tri");
	ASSERT_EQ(mesh.materialName, "metal");

	// Y-up to Z-up and meters to game units: glTF (0, 1, 0) is (0, 0, 1 m), and the winding is turned to clockwise
	expectVec(mesh.frames[0][2].pos, glm::vec3{0, 0, 1} * UNITS_PER_METER);
	ASSERT_EQ(mesh.tris[0], glm::ivec3(0, 2, 1));
	expectVec(mesh.frames[0][0].normal, {0, -1, 0});
	auto copy = *model;
	calculateNormals(copy);
	expectVec(copy.meshes[0].frames[0][0].normal, {0, -1, 0});

	// glTF has the texture origin at the top like the game
	ASSERT_EQ(mesh.texCoords[2], glm::vec2(0, 1));

	// The node moves along x
	expectVec(mesh.frames[12][1].pos, glm::vec3{2, 0, 0} * UNITS_PER_METER, 1e-3f);
	expectVec(mesh.frames[24][1].pos, glm::vec3{3, 0, 0} * UNITS_PER_METER, 1e-3f);

	ASSERT_EQ(model->tags.size(), 1);
	ASSERT_EQ(model->tags[0].name, "tag_flash");
	expectVec(model->tags[0].frames[0].location, glm::vec3{0, 0, 1} * UNITS_PER_METER, 1e-3f);
	expectVec(model->tags[0].frames[24].location, glm::vec3{2, 0, 1} * UNITS_PER_METER, 1e-3f);
}

TEST(ImportGLTFTest, skinned)
{
	auto model = load<skeletal::SkeletalModel>(textFile("arm.gltf", skinnedGltf()));
	ASSERT_TRUE(model);

	ASSERT_TRUE(model->hasBindPose);
	ASSERT_EQ(model->joints.size(), 2);
	ASSERT_EQ(model->joints[0].name, "root");
	ASSERT_EQ(model->joints[1].name, "arm");
	ASSERT_EQ(model->joints[1].parentId, 0);
	ASSERT_EQ(model->numFrames(), 25);

	const auto &mesh = model->meshes[0];
	ASSERT_EQ(mesh.vertices.size(), 3);
	ASSERT_EQ(mesh.vertices[2].weightCount, 2);

	// Bind pose: the arm bone is at x = 1
	expectVec(model->joints[1].location, glm::vec3{1, 0, 0} * UNITS_PER_METER, 1e-3f);
	expectVec(skeletal::posedVertex(*model, mesh, mesh.vertices[1], FRAME_BIND_POSE), glm::vec3{2, 0, 0} * UNITS_PER_METER, 1e-3f);
	expectVec(skeletal::posedVertex(*model, mesh, mesh.vertices[1], 0), glm::vec3{2, 0, 0} * UNITS_PER_METER, 1e-3f);

	// After turning 90 degrees around the glTF y axis the vertex is at glTF (1, 0, -1), which is (1, 1, 0) in Z-up
	expectVec(skeletal::posedVertex(*model, mesh, mesh.vertices[1], 24), glm::vec3{1, 1, 0} * UNITS_PER_METER, 1e-3f);
	// The root bound vertex does not move
	expectVec(skeletal::posedVertex(*model, mesh, mesh.vertices[0], 24), {0, 0, 0});
}

TEST(ImportGLTFTest, binary)
{
	GltfBuffers buffers;
	std::string json = staticGltf(false, &buffers);
	auto model = load<morph::MorphModel>(memoryFile("triangle.glb", glb(json, buffers.bin)));
	ASSERT_TRUE(model);
	ASSERT_EQ(model->numFrames(), 25);
	expectVec(model->meshes[0].frames[24][1].pos, glm::vec3{3, 0, 0} * UNITS_PER_METER, 1e-3f);

	// From the disk, the terminating null byte of the read file is not part of the GLB
	std::filesystem::create_directories("tmp/gltf");
	auto data = glb(json, buffers.bin);
	{
		std::ofstream out("tmp/gltf/triangle.glb", std::ios::binary);
		out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
	}
	mt::io::FileSystem disk;
	auto file = disk.loadFile(std::filesystem::absolute("tmp/gltf/triangle.glb").string());
	ASSERT_TRUE(file);
	EXPECT_EQ(file->len, data.size());
	EXPECT_NO_THROW(std::unique_ptr<Model>(mt::model::io::loadModel(file)));
	std::filesystem::remove_all("tmp");
}

TEST(ImportGLTFTest, errors)
{
	// External buffers are not read
	std::string json = staticGltf(false);
	auto external = json;
	auto pos = external.find("\"buffers\": [{\"byteLength\": ");
	ASSERT_NE(pos, std::string::npos);
	external.insert(external.find('}', pos), ", \"uri\": \"triangle.bin\"");
	try
	{
		loadGLTF(textFile("external.gltf", external));
		FAIL() << "expected an exception";
	}
	catch (const mt_exception &ex)
	{
		ASSERT_NE(std::string(ex.what()).find("External files are not supported"), std::string::npos) << ex.what();
	}

	ASSERT_THROW(loadGLTF(textFile("broken.gltf", "{ not json")), mt_exception);
	ASSERT_THROW(loadGLTF(textFile("empty.gltf", "{\"asset\": {\"version\": \"2.0\"}}")), mt_exception);

	// Accessor pointing outside its buffer
	auto broken = staticGltf();
	auto count = broken.find("\"count\": 3");
	ASSERT_NE(count, std::string::npos);
	broken.replace(count, 10, "\"count\": 300");
	ASSERT_THROW(loadGLTF(textFile("broken.gltf", broken)), mt_exception);
}

TEST(SceneImportTest, tagNames)
{
	ASSERT_TRUE(import::isTagName("tag_head"));
	ASSERT_TRUE(import::isTagName("TAG_Weapon"));
	ASSERT_FALSE(import::isTagName("tag"));
	ASSERT_FALSE(import::isTagName("head_tag"));
}
