#include "gtest/gtest.h"
#include "posed_model.h"
#include "playback.h"
#include "texture_lookup.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "morph_model.h"
#include "mt_path.h"

using namespace mt;
using namespace mt::model;

namespace {
	void expectVec(const glm::vec3 &actual, const glm::vec3 &expected, float tolerance = 1e-4f)
	{
		EXPECT_NEAR(actual.x, expected.x, tolerance);
		EXPECT_NEAR(actual.y, expected.y, tolerance);
		EXPECT_NEAR(actual.z, expected.z, tolerance);
	}

	std::unique_ptr<skeletal::SkeletalModel> loadBob()
	{
		mt::io::MTPath data("data");
		std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile("models/md5/bob_lamp_update_export.md5mesh")));
		model::io::loadAnimation(loaded.get(), data.loadFile("models/md5/bob_lamp_update_export.md5anim"));
		return std::unique_ptr<skeletal::SkeletalModel>(dynamic_cast<skeletal::SkeletalModel *>(loaded.release()));
	}

	/*! Two frames, the quad moves 10 units up, one tag */
	morph::MorphModel movingQuad()
	{
		morph::MorphModel model;
		model.frames.resize(2);

		morph::Mesh mesh;
		mesh.texCoords.resize(4);
		mesh.tris = {{0, 1, 2}, {0, 2, 3}};
		for (int frame = 0; frame < 2; frame++)
		{
			float z = frame * 10.0f;
			glm::vec3 turning = frame == 0 ? glm::vec3{1, 0, 0} : glm::vec3{0, 0, 1};
			mesh.frames.push_back({{{0, 0, z}, {0, 0, 1}}, {{1, 0, z}, {0, 0, 1}}, {{1, 1, z}, {0, 0, 1}}, {{0, 1, z}, turning}});
		}
		model.meshes.push_back(mesh);

		morph::Tag tag;
		tag.name = "tag_top";
		tag.frames = {{{0, 0, 0}, glm::quat(1, 0, 0, 0)}, {{0, 0, 10}, glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1})}};
		model.tags.push_back(tag);
		return model;
	}
}

TEST(PosedModelTest, skeletalMatchesPosedVertices)
{
	auto bob = loadBob();

	PoseOptions bind;
	bind.bindPose = true;
	auto posed = poseModel(*bob, bind);
	ASSERT_EQ(posed.meshes.size(), bob->meshes.size());
	ASSERT_EQ(posed.meshes[0].material, "bob_body");
	expectVec(posed.meshes[0].positions[0], bob->meshes[0].vertices[0].pos);

	PoseOptions frame;
	frame.frame = frame.nextFrame = 50;
	posed = poseModel(*bob, frame);
	for (size_t m = 0; m < bob->meshes.size(); m++)
	{
		for (size_t v = 0; v < bob->meshes[m].vertices.size(); v += 25)
		{
			expectVec(posed.meshes[m].positions[v], skeletal::posedVertex(*bob, bob->meshes[m], bob->meshes[m].vertices[v], 50));
			ASSERT_NEAR(glm::length(posed.meshes[m].normals[v]), 1.0f, 1e-4f);
		}
	}

	ASSERT_TRUE(posed.hasBounds);
	ASSERT_EQ(posed.joints.size(), bob->joints.size());
	ASSERT_EQ(posed.bones.size(), bob->joints.size() - 1); // every bone but the root
}

TEST(PosedModelTest, interpolation)
{
	auto bob = loadBob();

	PoseOptions a, b, half;
	a.frame = a.nextFrame = 10;
	b.frame = b.nextFrame = 11;
	half.frame = 10;
	half.nextFrame = 11;
	half.blend = 0.5f;

	auto posedA = poseModel(*bob, a), posedB = poseModel(*bob, b), posedHalf = poseModel(*bob, half);
	// The joints are blended, so the vertices land close to the middle
	for (size_t v = 0; v < posedA.meshes[0].positions.size(); v += 50)
	{
		glm::vec3 middle = (posedA.meshes[0].positions[v] + posedB.meshes[0].positions[v]) * 0.5f;
		expectVec(posedHalf.meshes[0].positions[v], middle, 0.05f);
	}
}

TEST(PosedModelTest, skeletalWithoutPose)
{
	// An MDM without an animation has nothing to draw
	mt::io::MTPath data("data");
	std::unique_ptr<Model> mdm(model::io::loadModel(data.loadFile("models/mdm/medic.mdm")));
	auto posed = poseModel(*mdm, PoseOptions{});
	ASSERT_TRUE(posed.meshes.empty());

	// With the animation the tags follow their bones
	model::io::loadAnimation(mdm.get(), data.loadFile("models/mdm/medic.mdx"));
	posed = poseModel(*mdm, PoseOptions{});
	ASSERT_EQ(posed.meshes.size(), 4);
	ASSERT_EQ(posed.tags.size(), 17);
	ASSERT_EQ(posed.tags[0].name, "tag_head");
	ASSERT_GT(posed.tags[0].point.location.z, 30.0f);
}

TEST(PosedModelTest, boneTagsAndHitboxes)
{
	skeletal::SkeletalModel model;
	model.hasBindPose = true;

	skeletal::Joint root;
	root.name = "root";
	root.parentId = -1;
	root.location = {0, 0, 10};
	model.joints.push_back(root);

	skeletal::Joint tag;
	tag.name = "tag_weapon";
	tag.parentId = 0;
	tag.asTag = true;
	tag.location = {5, 0, 10};
	model.joints.push_back(tag);

	Hitbox hitbox(4);
	hitbox.parentJoint = 0;
	hitbox.location = {0, 0, 1};
	model.hitboxes.push_back(hitbox);

	PoseOptions bind;
	bind.bindPose = true;
	auto posed = poseModel(model, bind);

	// Bones used as tags are tags, not part of the skeleton
	ASSERT_EQ(posed.joints.size(), 1);
	ASSERT_TRUE(posed.bones.empty());
	ASSERT_EQ(posed.tags.size(), 1);
	ASSERT_EQ(posed.tags[0].name, "tag_weapon");
	expectVec(posed.tags[0].point.location, {5, 0, 10});
	ASSERT_EQ(posed.tags[0].point.rotation, glm::quat(1, 0, 0, 0));

	model.flipBoneTags = true;
	posed = poseModel(model, bind);
	ASSERT_EQ(posed.tags[0].point.rotation, skeletal::BONE_TAG_FLIP);

	ASSERT_EQ(posed.hitboxes.size(), 1);
	expectVec(posed.hitboxes[0].point.location, {0, 0, 11});
	expectVec(posed.hitboxes[0].hitbox.max, {2, 2, 2});
}

TEST(PosedModelTest, morphFramesAndTags)
{
	auto model = movingQuad();

	PoseOptions options;
	options.frame = 0;
	options.nextFrame = 1;
	options.blend = 0.5f;
	auto posed = poseModel(model, options);

	ASSERT_EQ(posed.meshes.size(), 1);
	expectVec(posed.meshes[0].positions[2], {1, 1, 5});
	// Blended normals are normalized
	expectVec(posed.meshes[0].normals[3], glm::normalize(glm::vec3{1, 0, 1}));

	ASSERT_EQ(posed.tags.size(), 1);
	expectVec(posed.tags[0].point.location, {0, 0, 5});
	expectVec(posed.tags[0].point.rotation * glm::vec3{1, 0, 0}, glm::normalize(glm::vec3{1, 1, 0}));

	// Frames out of range are clamped
	options.frame = options.nextFrame = 5;
	options.blend = 0;
	posed = poseModel(model, options);
	expectVec(posed.meshes[0].positions[0], {0, 0, 10});
}

TEST(PosedModelTest, morphLevelsOfDetail)
{
	auto model = movingQuad();
	auto lod = model.meshes;
	lod[0].tris = {{0, 1, 2}};
	model.lods.push_back(lod);

	PoseOptions options;
	ASSERT_EQ(poseModel(model, options).meshes[0].tris.size(), 2);
	options.lodLevel = 1;
	ASSERT_EQ(poseModel(model, options).meshes[0].tris.size(), 1);
	options.lodLevel = 7; // clamped to the last level
	ASSERT_EQ(poseModel(model, options).meshes[0].tris.size(), 1);
}

TEST(PosedModelTest, collapseMaps)
{
	// Quad 0-1-2-3, vertex 3 collapses into 2, vertex 2 into 0
	std::vector<glm::ivec3> tris{{0, 1, 2}, {0, 2, 3}};
	std::vector<int> collapse{0, 0, 0, 2};

	ASSERT_EQ(lodTriangles(tris, collapse, 4), tris);
	ASSERT_EQ(lodTriangles(tris, {}, 2), tris);

	// Without vertex 3 the second triangle becomes 0-2-2 and is dropped
	auto three = lodTriangles(tris, collapse, 3);
	ASSERT_EQ(three.size(), 1);
	ASSERT_EQ(three[0], glm::ivec3(0, 1, 2));

	// Without vertex 2 nothing is left
	ASSERT_TRUE(lodTriangles(tris, collapse, 2).empty());

	// A broken map does not loop forever or read outside
	ASSERT_TRUE(lodTriangles(tris, {0, 0, 3, 2}, 2).empty());
	ASSERT_TRUE(lodTriangles(tris, {0, 0, 9, 2}, 2).empty());

	ASSERT_EQ(lodVertexCount(100, 40, 0.0f), 40);
	ASSERT_EQ(lodVertexCount(100, 40, 0.5f), 70);
	ASSERT_EQ(lodVertexCount(100, 40, 1.0f), 100);
	ASSERT_EQ(lodVertexCount(100, 140, 0.0f), 100);
}

TEST(PosedModelTest, dynamicLodOnRealModel)
{
	mt::io::MTPath data("data");
	std::unique_ptr<Model> medic(model::io::loadModel(data.loadFile("models/mdm/medic.mdm")));
	model::io::loadAnimation(medic.get(), data.loadFile("models/mdm/medic.mdx"));

	PoseOptions full, reduced;
	reduced.dynamicLod = true;
	reduced.lodRatio = 0.0f;
	size_t fullTris = poseModel(*medic, full).meshes[0].tris.size();
	size_t lodTris = poseModel(*medic, reduced).meshes[0].tris.size();
	ASSERT_GT(lodTris, 0);
	ASSERT_LT(lodTris, fullTris);
}

TEST(PlaybackTest, loopsAndBlends)
{
	Playback playback;
	playback.setRange(10, 13, 10, true); // 4 frames at 10 fps
	ASSERT_EQ(playback.frame(), 10);

	playback.advance(1.0); // not playing
	ASSERT_EQ(playback.frame(), 10);

	playback.play();
	playback.advance(0.15);
	ASSERT_EQ(playback.frame(), 11);
	ASSERT_EQ(playback.nextFrame(), 12);
	ASSERT_NEAR(playback.blend(), 0.5f, 1e-4f);

	playback.interpolate = false;
	ASSERT_EQ(playback.blend(), 0.0f);

	playback.advance(0.2); // frame 3.5 of 4: blends from the last frame back to the first
	ASSERT_EQ(playback.frame(), 13);
	ASSERT_EQ(playback.nextFrame(), 10);

	playback.advance(0.1); // wraps
	ASSERT_EQ(playback.frame(), 10);

	playback.speed = 2.0f;
	playback.advance(0.1);
	ASSERT_EQ(playback.frame(), 12);
}

TEST(PlaybackTest, stopsWithoutLoop)
{
	Playback playback;
	Animation animation{"death", 0, 3, 10, false};
	playback.setAnimation(animation);
	playback.play();
	playback.advance(10.0);
	ASSERT_EQ(playback.frame(), 2);
	ASSERT_EQ(playback.nextFrame(), 2);
	ASSERT_FALSE(playback.isPlaying());

	// Playing again starts over
	playback.play();
	ASSERT_EQ(playback.frame(), 0);
}

TEST(PlaybackTest, reversedAndSeek)
{
	Playback playback;
	Animation animation{"walk_back", 20, 5, 10, true};
	animation.reversed = true;
	playback.setAnimation(animation);
	ASSERT_EQ(playback.first(), 20);
	ASSERT_EQ(playback.last(), 16);

	playback.play();
	playback.advance(0.1);
	ASSERT_EQ(playback.frame(), 19);
	ASSERT_EQ(playback.nextFrame(), 18);

	playback.seek(17);
	ASSERT_EQ(playback.frame(), 17);
	playback.seek(100); // clamped to the range
	ASSERT_EQ(playback.frame(), 20);
	playback.seek(0);
	ASSERT_EQ(playback.frame(), 16);
}

TEST(TextureLookupTest, candidates)
{
	auto list = mt::io::textureCandidates("models\\weapons2\\pf.tga", "models/weapons2/pf.md3");
	ASSERT_EQ(list[0], "models/weapons2/pf.tga");
	ASSERT_NE(std::find(list.begin(), list.end(), "models/weapons2/pf.jpg"), list.end());

	// Absolute paths from the artist's machine
	list = mt::io::textureCandidates("C:\\mapping\\etmain\\models/players/body.tga", "");
	ASSERT_NE(std::find(list.begin(), list.end(), "models/players/body.tga"), list.end());
	ASSERT_NE(std::find(list.begin(), list.end(), "models/players/body.jpg"), list.end());

	// Stacked extensions and textures next to the model
	list = mt::io::textureCandidates("textures/Head.tga.png", "models/fixit/fixit.fbx");
	ASSERT_NE(std::find(list.begin(), list.end(), "models/fixit/Head.tga"), list.end());

	ASSERT_TRUE(mt::io::textureCandidates("", "x").empty());

	// No duplicates
	std::set<std::string> unique(list.begin(), list.end());
	ASSERT_EQ(unique.size(), list.size());
}

TEST(TextureLookupTest, findsTexturesNextToTheModel)
{
	mt::io::MTPath data("data");
	ASSERT_EQ(mt::io::findTexture(data, "bob_body", "models/md5/bob_lamp_update_export.md5mesh"), "models/md5/bob_body.tga");
	ASSERT_EQ(mt::io::findTexture(data, "models/md5/lantern.jpg", ""), "models/md5/lantern.tga");
	ASSERT_EQ(mt::io::findTexture(data, "missing", "models/md5/x.md5mesh"), "");
}

TEST(TextureLookupTest, picmip)
{
	mt::io::Image image;
	image.width = 64;
	image.height = 32;
	image.pixels.resize(64 * 32 * 4);
	for (size_t i = 0; i < image.pixels.size(); i++)
	{
		image.pixels[i] = byte((i / 4) % 2 ? 255 : 0); // alternating black and white columns
	}

	auto once = mt::io::applyPicmip(image, 1);
	ASSERT_EQ(once.width, 32);
	ASSERT_EQ(once.height, 16);
	ASSERT_EQ(once.pixels.size(), size_t(32 * 16 * 4));
	ASSERT_EQ(once.pixels[0], 128); // averaged

	// Not below the minimum size
	auto many = mt::io::applyPicmip(image, 5);
	ASSERT_EQ(many.height, 16);
	ASSERT_EQ(mt::io::applyPicmip(image, 0).width, 64);
}
