#include "gtest/gtest.h"
#include "filesystem.h"
#include "mt.h"
#include "hitbox.h"
#include "mt_path.h"
#include <filesystem>

using namespace mt;

TEST(HitboxTest, ReadTest)
{
	io::MTPath path("data");
	std::vector<model::Hitbox> hitboxes{};

	ASSERT_NO_THROW(model::Hitbox::loadHitboxes(path.loadFile("test/test.hitbox"), hitboxes););

	ASSERT_EQ(hitboxes.size(), 3);
	ASSERT_EQ(hitboxes[0].name, "Test name");
	ASSERT_EQ(hitboxes[0].modifiers.size(), 1);
	ASSERT_EQ(*hitboxes[0].modifiers.begin(), "headshot");
	EXPECT_FLOAT_EQ(hitboxes[0].location.x, 0.1f);
	EXPECT_FLOAT_EQ(hitboxes[0].location.y, 0.2f);
	EXPECT_FLOAT_EQ(hitboxes[0].location.z, 0.3f);

	ASSERT_EQ(hitboxes[1].name, "Test name 2");
	EXPECT_FLOAT_EQ(hitboxes[1].location.x, 0.7f);
	EXPECT_FLOAT_EQ(hitboxes[1].location.y, 0.6f);
	EXPECT_FLOAT_EQ(hitboxes[1].location.z, 0.5f);

	ASSERT_EQ(hitboxes[2].name, "Test name 3");
	// The file has min and max swapped, loading sorts them
	EXPECT_EQ(hitboxes[2].shape, model::HitboxShape::Box);
	EXPECT_FLOAT_EQ(hitboxes[2].min.x, 0.3f);
	EXPECT_FLOAT_EQ(hitboxes[2].min.y, 0.2f);
	EXPECT_FLOAT_EQ(hitboxes[2].min.z, 0.1f);

	EXPECT_FLOAT_EQ(hitboxes[2].max.x, 0.6f);
	EXPECT_FLOAT_EQ(hitboxes[2].max.y, 0.5f);
	EXPECT_FLOAT_EQ(hitboxes[2].max.z, 0.4f);

	emptyVector(hitboxes);
}

TEST(HitboxTest, WriteTest)
{
	io::FileSystem system;

	const auto &tmpPath = std::filesystem::temp_directory_path();

	std::filesystem::path hitboxFile = tmpPath / "test.hitboxes";

	std::vector<model::Hitbox> hitboxes;

	for (int i = 0; i < 5; i++)
	{
		model::Hitbox box;
		box.name = "Hitbox: " + std::to_string(i + 1);
		box.location = glm::vec3(0.1f, 0.2f, 0.3f) * float(i + 1);
		box.rotation = glm::quat(0.1f, 0.2f, 0.3f, 0.4f) * float(i + 1);

		if (i == 0)
		{
			box.modifiers.emplace("headshot");
		}

		hitboxes.emplace_back(box);
	}

	ASSERT_NO_THROW(model::Hitbox::saveHitboxes(hitboxFile.string(), hitboxes););
	emptyVector(hitboxes);
	ASSERT_EQ(hitboxes.size(), 0);

	ASSERT_NO_THROW(model::Hitbox::loadHitboxes(system.loadFile(hitboxFile.string()), hitboxes););

	ASSERT_EQ(hitboxes.size(), 5);

	ASSERT_EQ(hitboxes[0].name, "Hitbox: 1");
	ASSERT_EQ(*hitboxes[0].modifiers.begin(), "headshot");

	ASSERT_EQ(hitboxes[4].name, "Hitbox: 5");
	ASSERT_FLOAT_EQ(hitboxes[4].location.x, 0.5f);
	ASSERT_FLOAT_EQ(hitboxes[4].location.y, 1.0f);
	ASSERT_FLOAT_EQ(hitboxes[4].location.z, 1.5f);

	ASSERT_FLOAT_EQ(hitboxes[4].rotation.x, 1.0f);
	ASSERT_FLOAT_EQ(hitboxes[4].rotation.y, 1.5f);
	ASSERT_FLOAT_EQ(hitboxes[4].rotation.z, 2.0f);
	ASSERT_FLOAT_EQ(hitboxes[4].rotation.w, 0.5f);

	std::filesystem::remove(hitboxFile);
	emptyVector(hitboxes);
}

TEST(HitboxTest, ShapeRoundTrip)
{
	io::FileSystem system;
	std::filesystem::create_directories("tmp");
	const std::string file = "tmp/shapes.hitboxes";

	std::vector<model::Hitbox> hitboxes;
	for (auto shape : {model::HitboxShape::Box, model::HitboxShape::Sphere, model::HitboxShape::Cylinder, model::HitboxShape::Capsule})
	{
		model::Hitbox box(4);
		box.name = model::hitboxShapeName(shape);
		box.parentJoint = 2;
		box.setShape(shape);
		box.radius = 1.25f;
		box.height = 6.5f;
		hitboxes.push_back(box);
	}

	ASSERT_NO_THROW(model::Hitbox::saveHitboxes(file, hitboxes););
	std::vector<model::Hitbox> loaded;
	ASSERT_NO_THROW(model::Hitbox::loadHitboxes(system.loadFile(file), loaded););
	std::filesystem::remove_all("tmp");

	ASSERT_EQ(loaded.size(), 4);
	EXPECT_EQ(loaded[0].shape, model::HitboxShape::Box);
	EXPECT_FLOAT_EQ(loaded[0].min.x, -2);
	EXPECT_FLOAT_EQ(loaded[0].max.z, 2);
	EXPECT_EQ(loaded[1].shape, model::HitboxShape::Sphere);
	EXPECT_FLOAT_EQ(loaded[1].radius, 1.25f);
	EXPECT_EQ(loaded[2].shape, model::HitboxShape::Cylinder);
	EXPECT_FLOAT_EQ(loaded[2].radius, 1.25f);
	EXPECT_FLOAT_EQ(loaded[2].height, 6.5f);
	EXPECT_EQ(loaded[3].shape, model::HitboxShape::Capsule);
	EXPECT_FLOAT_EQ(loaded[3].height, 6.5f);
	EXPECT_EQ(loaded[3].parentJoint, 2);
	EXPECT_EQ(loaded[3].name, "capsule");
}

TEST(HitboxTest, ShapeConversion)
{
	model::Hitbox box(0);
	box.min = {-1, -2, -3};
	box.max = {1, 2, 3};

	model::Hitbox cylinder = box;
	cylinder.setShape(model::HitboxShape::Cylinder);
	EXPECT_FLOAT_EQ(cylinder.radius, 2);
	EXPECT_FLOAT_EQ(cylinder.height, 6);

	model::Hitbox sphere = box;
	sphere.setShape(model::HitboxShape::Sphere);
	EXPECT_FLOAT_EQ(sphere.radius, 3);

	// A capsule is at least as high as it is wide
	model::Hitbox capsule = box;
	capsule.radius = 5;
	capsule.setShape(model::HitboxShape::Capsule);
	capsule.radius = 4;
	capsule.clampSize();
	EXPECT_FLOAT_EQ(capsule.height, 8);

	capsule.setShape(model::HitboxShape::Box);
	EXPECT_FLOAT_EQ(capsule.min.x, -4);
	EXPECT_FLOAT_EQ(capsule.max.z, 4);
}

TEST(HitboxTest, ShapeLines)
{
	for (auto shape : {model::HitboxShape::Sphere, model::HitboxShape::Cylinder, model::HitboxShape::Capsule})
	{
		model::Hitbox hitbox(0);
		hitbox.shape = shape;
		hitbox.radius = 2;
		hitbox.height = 10;

		auto lines = hitbox.lines(16);
		ASSERT_FALSE(lines.empty());
		Bounds bounds = hitbox.localBounds();
		for (const auto &[a, b] : lines)
		{
			for (const auto &p : {a, b})
			{
				// Every point is inside the bounds and on the surface of the shape
				EXPECT_TRUE(glm::all(glm::greaterThanEqual(p, bounds.min - 1e-4f)));
				EXPECT_TRUE(glm::all(glm::lessThanEqual(p, bounds.max + 1e-4f)));
				float distance = 0;
				switch (shape)
				{
				case model::HitboxShape::Sphere: distance = glm::length(p); break;
				case model::HitboxShape::Cylinder: distance = std::abs(p.z) < 5 - 1e-4f ? glm::length(glm::vec2(p)) : 2; break;
				default: distance = glm::length(p - glm::vec3(0, 0, glm::clamp(p.z, -3.0f, 3.0f))); break;
				}
				EXPECT_NEAR(distance, 2, 1e-4f);
			}
		}
	}

	model::Hitbox box(2);
	EXPECT_EQ(box.lines().size(), 12);
}
