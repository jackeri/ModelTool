#include "gtest/gtest.h"
#include "skeletal_model.h"
#include "morph_model.h"

using namespace mt;
using namespace mt::model;

TEST(ModelTypesTest, skeletalVertexCopyAndAssign)
{
	skeletal::SkeletalVertex vert;
	vert.pos = {1, 2, 3};
	vert.normal = {0, 0, 1};
	vert.tex = {0.25f, 0.75f};
	vert.startWeight = 4;
	vert.weightCount = 2;
	vert.bindNormal = {0, 1, 0};

	skeletal::SkeletalVertex copied(vert);
	skeletal::SkeletalVertex assigned;
	assigned = vert;

	for (const auto *other : {&copied, &assigned})
	{
		ASSERT_EQ(other->pos, vert.pos);
		ASSERT_EQ(other->normal, vert.normal);
		ASSERT_EQ(other->tex, vert.tex);
		ASSERT_EQ(other->startWeight, vert.startWeight);
		ASSERT_EQ(other->weightCount, vert.weightCount);
		ASSERT_EQ(other->bindNormal, vert.bindNormal);
	}

	// vectors use assignment when inserting into the middle
	std::vector<skeletal::SkeletalVertex> list(3);
	list.insert(list.begin(), vert);
	ASSERT_EQ(list[0].startWeight, 4);
	ASSERT_EQ(list[0].bindNormal, vert.bindNormal);
}

TEST(ModelTypesTest, defaults)
{
	skeletal::SkeletalModel model;
	ASSERT_FLOAT_EQ(model.scale, 1.0f);
	ASSERT_EQ(model.torsoParent, -1);
	ASSERT_TRUE(model.animations.empty());

	Point point;
	ASSERT_EQ(point.rotation, glm::quat(1, 0, 0, 0));
}

TEST(ModelTypesTest, pointAxisRoundTrip)
{
	glm::quat rotation = glm::normalize(glm::angleAxis(glm::radians(30.0f), glm::vec3{0, 0, 1}));
	Point point{{1, 2, 3}, rotation};

	glm::mat3 axis = point.axis();
	// axis[0] is the rotated forward vector
	glm::vec3 forward = rotation * glm::vec3{1, 0, 0};
	ASSERT_NEAR(axis[0].x, forward.x, 1e-5f);
	ASSERT_NEAR(axis[0].y, forward.y, 1e-5f);

	Point back = Point::fromAxis(point.location, axis);
	ASSERT_NEAR(glm::abs(glm::dot(back.rotation, rotation)), 1.0f, 1e-5f);
}

TEST(ModelTypesTest, animationLastFrame)
{
	Animation forward{"run", 10, 5};
	ASSERT_EQ(forward.lastFrame(), 14);

	Animation backward{"walk_back", 10, 5};
	backward.reversed = true;
	ASSERT_EQ(backward.lastFrame(), 6);
}

static morph::MorphModel makeMorphModel()
{
	morph::MorphModel model;
	model.frames.resize(2);

	morph::Mesh mesh;
	mesh.name = "quad";
	mesh.texCoords = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	mesh.tris = {{0, 1, 2}, {0, 2, 3}};
	for (int frame = 0; frame < 2; frame++)
	{
		float z = float(frame);
		mesh.frames.push_back({{{0, 0, z}, {}}, {{1, 0, z}, {}}, {{1, 1, z}, {}}, {{0, 1, z}, {}}});
	}
	model.meshes.push_back(mesh);

	morph::Tag tag;
	tag.name = "tag_head";
	tag.frames.resize(2);
	model.tags.push_back(tag);

	return model;
}

TEST(ModelTypesTest, morphModel)
{
	auto model = makeMorphModel();

	ASSERT_TRUE(model.morphModel());
	ASSERT_FALSE(model.skeletalModel());
	ASSERT_EQ(model.numFrames(), 2);
	ASSERT_TRUE(model.hasDrawable());
	ASSERT_TRUE(model.hasTags());
	ASSERT_EQ(model.currentFrame, 0);
	ASSERT_NO_THROW(model.validate());

	model.currentFrame = FRAME_BIND_POSE;
	ASSERT_EQ(model.frameIndex(), 0);
	model.currentFrame = 10;
	ASSERT_EQ(model.frameIndex(), 1);

	ASSERT_EQ(model.lodCount(), 1);
	ASSERT_EQ(&model.lod(0), &model.meshes);
	ASSERT_THROW(model.lod(1), mt_exception);
	model.lods.push_back(model.meshes);
	ASSERT_EQ(model.lodCount(), 2);
	ASSERT_EQ(model.lod(1).size(), 1);
}

TEST(ModelTypesTest, morphModelValidation)
{
	auto missingFrame = makeMorphModel();
	missingFrame.meshes[0].frames.pop_back();
	ASSERT_THROW(missingFrame.validate(), mt_exception);

	auto badTriangle = makeMorphModel();
	badTriangle.meshes[0].tris.push_back({0, 1, 4});
	ASSERT_THROW(badTriangle.validate(), mt_exception);

	auto badTag = makeMorphModel();
	badTag.tags[0].frames.pop_back();
	ASSERT_THROW(badTag.validate(), mt_exception);
}
