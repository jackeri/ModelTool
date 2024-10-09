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
	EXPECT_FLOAT_EQ(hitboxes[2].min.x, 0.6f);
	EXPECT_FLOAT_EQ(hitboxes[2].min.y, 0.5f);
	EXPECT_FLOAT_EQ(hitboxes[2].min.z, 0.4f);

	EXPECT_FLOAT_EQ(hitboxes[2].max.x, 0.3f);
	EXPECT_FLOAT_EQ(hitboxes[2].max.y, 0.2f);
	EXPECT_FLOAT_EQ(hitboxes[2].max.z, 0.1f);

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
