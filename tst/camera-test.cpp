#include "gtest/gtest.h"
#include "camera.h"

using namespace mt;

TEST(CameraTest, ViewFromAxis)
{
	// The view looks along -Z: the axis we look from points at the camera (+Z in the view)
	const glm::vec3 toCamera{0, 0, 1}, screenUp{0, 1, 0};
	auto near = [](const glm::vec3 &a, const glm::vec3 &b) { return glm::length(a - b) < 1e-4f; };

	Camera camera;
	for (int axis = 0; axis < 3; axis++)
	{
		for (bool negative : {false, true})
		{
			camera.viewFrom(axis, negative);
			glm::vec3 direction{0.0f};
			direction[axis] = negative ? -1.0f : 1.0f;
			EXPECT_TRUE(near(camera.rotation() * direction, toCamera)) << axis << negative;

			// Up on the screen: +Z for the side views, +X (forward) for the top and bottom views
			const glm::vec3 up = axis == 2 ? glm::vec3{1, 0, 0} : glm::vec3{0, 0, 1};
			EXPECT_TRUE(near(camera.rotation() * up, screenUp)) << axis << negative;
			EXPECT_FLOAT_EQ(camera.angles.z, 0.0f);
		}
	}
}
