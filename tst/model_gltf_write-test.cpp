#include "gtest/gtest.h"
#include "filesystem.h"
#include "model_gltf_write.h"
#include "model_mdx.h"
#include "model_scripts.h"
#include "morph_model.h"
#include "posed_model.h"
#include "skeletal_model.h"
#include "skin.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace mt;
using namespace mt::model;

namespace {
	std::string jsonOf(const std::vector<byte> &glb)
	{
		// GLB: 12 byte header, then the JSON chunk length and type
		const uint32_t length = uint32_t(glb[12]) | uint32_t(glb[13]) << 8 | uint32_t(glb[14]) << 16 | uint32_t(glb[15]) << 24;
		return std::string(glb.begin() + 20, glb.begin() + 20 + length);
	}

	size_t count(const std::string &text, const std::string &part)
	{
		size_t found = 0;
		for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
		{
			found++;
		}
		return found;
	}

	/*! Write the data to tmp/gltf and load it back with the glTF importer */
	std::unique_ptr<Model> reload(const std::vector<byte> &data, const std::string &name)
	{
		std::filesystem::create_directories("tmp/gltf");
		const std::string path = "tmp/gltf/" + name;
		{
			std::ofstream out(path, std::ios::binary);
			out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
		}
		mt::io::FileSystem disk;
		return std::unique_ptr<Model>(mt::model::io::loadModel(disk.loadFile(std::filesystem::absolute(path).string())));
	}

	Bounds meshBounds(const PosedModel &posed, const glm::mat4 &matrix = glm::mat4(1.0f))
	{
		Bounds bounds{glm::vec3(std::numeric_limits<float>::max()), glm::vec3(-std::numeric_limits<float>::max())};
		for (const auto &mesh : posed.meshes)
		{
			for (const auto &position : mesh.positions)
			{
				const glm::vec3 point = glm::vec3(matrix * glm::vec4(position, 1.0f));
				bounds.min = glm::min(bounds.min, point);
				bounds.max = glm::max(bounds.max, point);
			}
		}
		return bounds;
	}

	struct Fixtures {
		mt::io::FileSystem files;
		Fixtures()
		{
			files.addPath("data");
		}

		std::unique_ptr<Model> load(const std::string &path)
		{
			return std::unique_ptr<Model>(mt::model::io::loadModel(files.loadFile(path)));
		}
	};
}

TEST(GltfWriteTest, SkeletalRoundTrip)
{
	Fixtures fixtures;
	auto original = fixtures.load("models/mdm/mannequin.mdm");
	loadMDXAnimation(original.get(), fixtures.files.loadFile("models/mdm/mannequin.mdx"));
	loadAninc(original.get(), fixtures.files.loadFile("models/mdm/mannequin.aninc"));
	auto &mannequin = dynamic_cast<skeletal::SkeletalModel &>(*original);
	ASSERT_FALSE(mannequin.animations.empty());

	// In meters, the default: the skeleton must not get a scale (importers turn joint matrices into rotations)
	GltfOptions options;
	GltfPart part;
	part.model = &mannequin;
	part.name = "mannequin";
	const auto data = encodeGLTF({part}, options);

	// A GLB file
	ASSERT_GT(data.size(), 20);
	EXPECT_EQ(std::string(data.begin(), data.begin() + 4), "glTF");
	const std::string json = jsonOf(data);
	EXPECT_EQ(count(json, "\"skins\""), 1);
	EXPECT_EQ(count(json, "\"name\":\"walk\""), 1);

	auto loaded = reload(data, "mannequin.glb");
	auto *imported = dynamic_cast<skeletal::SkeletalModel *>(loaded.get());
	ASSERT_TRUE(imported);
	ASSERT_EQ(imported->meshes.size(), mannequin.meshes.size());
	ASSERT_EQ(imported->animations.size(), mannequin.animations.size());

	// Every sequence at the times both frame rates have (20 fps in the model, 24 in the import): the same mesh
	for (size_t a = 0; a < mannequin.animations.size(); a++)
	{
		const auto &source = mannequin.animations[a];
		const auto &target = imported->animations[a];
		EXPECT_EQ(target.name, source.name);
		for (int k = 0; k < source.numFrames; k += 5)
		{
			PoseOptions before, after;
			before.frame = before.nextFrame = source.startFrame + k;
			after.frame = after.nextFrame = target.startFrame + k * 24 / 20;
			const auto a0 = poseModel(mannequin, before);
			const auto b0 = poseModel(*imported, after);
			for (size_t m = 0; m < a0.meshes.size(); m++)
			{
				for (size_t v = 0; v < a0.meshes[m].positions.size(); v += 11)
				{
					const glm::vec3 p = a0.meshes[m].positions[v];
					const glm::vec3 q = b0.meshes[m].positions[v] / METERS_PER_UNIT;
					ASSERT_LT(glm::distance(p, q), 0.005f) << source.name << " frame " << k << " mesh " << m << " vertex " << v;
				}
			}
		}
	}
}

TEST(GltfWriteTest, MetersAndMorphTargets)
{
	Fixtures fixtures;
	auto tube = fixtures.load("models/md3/tube.md3");
	auto &morph = dynamic_cast<morph::MorphModel &>(*tube);
	ASSERT_EQ(morph.numFrames(), 3);

	GltfPart part;
	part.model = tube.get();
	part.name = "tube";
	const auto data = encodeGLTF({part}, GltfOptions{});
	const std::string json = jsonOf(data);

	// The three frames are morph targets, played by one animation; the tag is a node
	EXPECT_EQ(count(json, "\"targets\""), morph.meshes.size());
	EXPECT_EQ(count(json, "\"path\":\"weights\""), morph.meshes.size());
	EXPECT_EQ(count(json, "\"name\":\"frames\""), 1);
	EXPECT_EQ(count(json, "\"name\":\"" + morph.tags[0].name + "\""), 1);

	// The importer reads the first frame, in meters
	auto loaded = reload(data, "tube.glb");
	ASSERT_TRUE(loaded);
	PoseOptions first;
	const Bounds original = meshBounds(poseModel(morph, first));
	const Bounds imported = meshBounds(poseModel(*loaded, first));
	for (int axis = 0; axis < 3; axis++)
	{
		EXPECT_NEAR(imported.min[axis], original.min[axis] * METERS_PER_UNIT, 1e-4f);
		EXPECT_NEAR(imported.max[axis], original.max[axis] * METERS_PER_UNIT, 1e-4f);
	}

	// Without morph targets only the first frame
	GltfOptions still;
	still.morphTargets = false;
	EXPECT_EQ(count(jsonOf(encodeGLTF({part}, still)), "\"targets\""), 0);
}

TEST(GltfWriteTest, PlayerWithAttachmentsAndTextures)
{
	Fixtures fixtures;
	auto lower = fixtures.load("models/players/robot/lower.md3");
	auto upper = fixtures.load("models/players/robot/upper.md3");
	auto head = fixtures.load("models/players/robot/head.md3");
	const Skin lowerSkin = loadSkin(fixtures.files.loadFile("models/players/robot/lower_red.skin"));
	const Skin upperSkin = loadSkin(fixtures.files.loadFile("models/players/robot/upper_red.skin"));
	const Skin headSkin = loadSkin(fixtures.files.loadFile("models/players/robot/head_red.skin"));

	std::vector<GltfPart> parts(3);
	parts[0] = {lower.get(), "lower", "models/players/robot/lower.md3", -1, "", &lowerSkin};
	parts[1] = {upper.get(), "upper", "models/players/robot/upper.md3", 0, "tag_torso", &upperSkin};
	parts[2] = {head.get(), "head", "models/players/robot/head.md3", 1, "TAG_HEAD", &headSkin};

	GltfOptions options;
	options.scale = 1.0f;
	options.files = &fixtures.files;
	std::vector<std::string> warnings;
	options.warn = [&](const std::string &text) { warnings.push_back(text); };
	const auto data = encodeGLTF(parts, options);
	const std::string json = jsonOf(data);
	EXPECT_TRUE(warnings.empty()) << warnings.front();

	// The skins' textures, converted from TGA to PNG and embedded
	EXPECT_EQ(count(json, "\"mimeType\":\"image/png\""), 2);				// red.tga and red_h.tga
	EXPECT_EQ(count(json, "\"name\":\"models/players/robot/red.tga\""), 2); // material and image
	// The animations of the parts after the main one are named after the part
	EXPECT_GT(count(json, "\"name\":\"upper: "), 0);

	// The parts sit on their tags at the first frame
	PoseOptions first;
	const auto lowerPose = poseModel(*lower, first);
	const auto upperPose = poseModel(*upper, first);
	glm::mat4 torso{1.0f}, neck{1.0f};
	ASSERT_TRUE(tagMatrix(lowerPose, "tag_torso", torso));
	ASSERT_TRUE(tagMatrix(upperPose, "tag_head", neck));
	Bounds expected = meshBounds(lowerPose);
	for (const Bounds &part : {meshBounds(upperPose, torso), meshBounds(poseModel(*head, first), torso * neck)})
	{
		expected.min = glm::min(expected.min, part.min);
		expected.max = glm::max(expected.max, part.max);
	}

	auto loaded = reload(data, "robot.glb");
	ASSERT_TRUE(loaded);
	const Bounds imported = meshBounds(poseModel(*loaded, first));
	for (int axis = 0; axis < 3; axis++)
	{
		EXPECT_NEAR(imported.min[axis], expected.min[axis], 1e-3f);
		EXPECT_NEAR(imported.max[axis], expected.max[axis], 1e-3f);
	}

	// A missing attach point is reported and the part goes on the parent's origin
	parts[2].attachPoint = "tag_nothing";
	encodeGLTF(parts, options);
	ASSERT_FALSE(warnings.empty());
	EXPECT_NE(warnings.back().find("tag_nothing"), std::string::npos);

	parts[1].parent = 2;
	EXPECT_THROW(encodeGLTF(parts, options), mt_exception);
	EXPECT_THROW(encodeGLTF({}, options), mt_exception);
}

TEST(GltfWriteTest, TextFile)
{
	Fixtures fixtures;
	auto tube = fixtures.load("models/md3/tube.md3");
	std::filesystem::create_directories("tmp/gltf");
	writeGLTF(*tube, "tmp/gltf/tube.gltf");

	std::string text;
	{
		// Closed before the folder is removed: Windows does not remove open files
		std::ifstream in("tmp/gltf/tube.gltf");
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	EXPECT_EQ(text.rfind("{\"asset\"", 0), 0);
	EXPECT_NE(text.find("data:application/octet-stream;base64,"), std::string::npos);

	mt::io::FileSystem disk;
	std::unique_ptr<Model> loaded(mt::model::io::loadModel(disk.loadFile(std::filesystem::absolute("tmp/gltf/tube.gltf").string())));
	EXPECT_TRUE(loaded);
	std::filesystem::remove_all("tmp/gltf");
}
