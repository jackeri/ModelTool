#include "gtest/gtest.h"
#include "measure.h"

using namespace mt;
using namespace mt::model;

TEST(MeasureTest, Format)
{
	// An ET player is about 72 units tall: 72 inches
	EXPECT_EQ(formatLength(72.0f, LengthUnit::Game), "72.0 u");
	EXPECT_EQ(formatLength(72.0f, LengthUnit::Metric), "1.83 m");
	EXPECT_EQ(formatLength(18.0f, LengthUnit::Metric), "45.7 cm");
	EXPECT_EQ(formatLength(72.0f, LengthUnit::Imperial), "6 ft 0.0 in");
	EXPECT_EQ(formatLength(70.5f, LengthUnit::Imperial), "5 ft 10.5 in");
	EXPECT_EQ(formatLength(8.5f, LengthUnit::Imperial), "8.5 in");
	// Rounding does not give "0 ft 12.0 in" or "11 ft 12.0 in"
	EXPECT_EQ(formatLength(11.96f, LengthUnit::Imperial), "1 ft 0.0 in");
	EXPECT_EQ(formatLength(143.97f, LengthUnit::Imperial), "12 ft 0.0 in");
	EXPECT_EQ(formatLength(0.0f, LengthUnit::Game), "0.0 u");
}

TEST(MeasureTest, Lines)
{
	const auto lines = dimensionLines({{-10, -5, 0}, {10, 5, 72}}, 2.0f);
	EXPECT_EQ(std::string(dimensionName(lines[2].axis)), "Height");
	EXPECT_FLOAT_EQ(lines[0].length, 20.0f);
	EXPECT_FLOAT_EQ(lines[1].length, 10.0f);
	EXPECT_FLOAT_EQ(lines[2].length, 72.0f);

	// Outside the box, parallel to their axis, with the label in the middle
	EXPECT_EQ(lines[0].from, glm::vec3(-10, -7, 0));
	EXPECT_EQ(lines[0].to, glm::vec3(10, -7, 0));
	EXPECT_EQ(lines[1].from, glm::vec3(12, -5, 0));
	EXPECT_EQ(lines[2].to, glm::vec3(12, -7, 72));
	EXPECT_EQ(lines[2].label, glm::vec3(12, -7, 36));

	// Extension lines start at the box and go past the dimension line
	ASSERT_EQ(lines[1].extensions.size(), 2);
	EXPECT_EQ(lines[1].extensions[1].first, glm::vec3(10, 5, 0));
	EXPECT_EQ(lines[1].extensions[1].second, glm::vec3(13, 5, 0));
}
