#include "gtest/gtest.h"
#include "point_edit.h"

using namespace mt;
using namespace mt::model;

namespace {
	bool near(const glm::vec3 &a, const glm::vec3 &b)
	{
		return glm::length(a - b) < 1e-4f;
	}

	bool near(const glm::quat &a, const glm::quat &b)
	{
		return std::abs(glm::dot(a, b)) > 1.0f - 1e-5f;
	}
}

TEST(PointEditTest, Matrices)
{
	Point point;
	point.location = {1, 2, 3};
	point.rotation = glm::angleAxis(glm::radians(30.0f), glm::normalize(glm::vec3{1, 1, 0}));

	Point back = pointFromMatrix(pointMatrix(point));
	EXPECT_TRUE(near(back.location, point.location));
	EXPECT_TRUE(near(back.rotation, point.rotation));

	// Scale (like the model scale) is dropped from the rotation
	back = pointFromMatrix(pointMatrix(point) * glm::scale(glm::vec3(2.0f)));
	EXPECT_TRUE(near(back.rotation, point.rotation));
}

TEST(PointEditTest, LocalPoint)
{
	// A tag on a bone, the model scaled and moved: moving it in the world gives the new offset from the bone
	Point bone;
	bone.location = {10, 0, 5};
	bone.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1});
	const glm::mat4 model = glm::translate(glm::vec3{0, 0, 100}) * glm::scale(glm::vec3(2.0f));
	const glm::mat4 parent = model * pointMatrix(bone);

	Point tag;
	tag.location = {1, 0, 0};
	const glm::mat4 world = parent * pointMatrix(tag);
	EXPECT_TRUE(near(glm::vec3(world[3]), {20, 2, 110}));

	// Unchanged world matrix gives the same point back
	Point same = localPoint(parent, world);
	EXPECT_TRUE(near(same.location, tag.location));
	EXPECT_TRUE(near(same.rotation, tag.rotation));

	// Moved up by 4 world units = 2 model units, along the bone's Z
	Point moved = localPoint(parent, glm::translate(glm::vec3{0, 0, 4}) * world);
	EXPECT_TRUE(near(moved.location, {1, 0, 2}));

	// Turned 90 degrees around the world Z at the tag
	const glm::mat4 turned = world * glm::toMat4(glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1}));
	Point rotated = localPoint(parent, turned);
	EXPECT_TRUE(near(rotated.location, tag.location));
	EXPECT_TRUE(near(rotated.rotation, glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1})));
}

TEST(PointEditTest, MoveMorphTag)
{
	morph::MorphModel model;
	morph::Tag tag;
	tag.name = "tag_weapon";
	for (int i = 0; i < 3; i++)
	{
		Point point;
		point.location = {float(i), 0, 0};
		tag.frames.push_back(point);
	}
	model.tags.push_back(tag);

	Point edited;
	edited.location = {1, 5, 0};
	edited.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0, 0, 1});

	// Only the edited frame
	auto copy = model;
	moveMorphTag(copy, 0, 1, edited, false);
	EXPECT_TRUE(near(copy.tags[0].frames[1].location, {1, 5, 0}));
	EXPECT_TRUE(near(copy.tags[0].frames[0].location, {0, 0, 0}));

	// Every frame moves the same way: up by 5 and turned around the edited frame's point
	moveMorphTag(model, 0, 1, edited, true);
	EXPECT_TRUE(near(model.tags[0].frames[1].location, {1, 5, 0}));
	EXPECT_TRUE(near(model.tags[0].frames[0].location, {1, 4, 0}));
	EXPECT_TRUE(near(model.tags[0].frames[2].location, {1, 6, 0}));
	EXPECT_TRUE(near(model.tags[0].frames[2].rotation, edited.rotation));

	// Out of range is ignored
	moveMorphTag(model, 3, 0, edited, true);
	moveMorphTag(model, 0, 9, edited, true);
}
