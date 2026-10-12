#include "gtest/gtest.h"
#include "embedded_files.h"

using namespace mt;

TEST(EmbeddedFilesTest, Fonts)
{
	for (const char *name : {"Hack-Regular.ttf", "fontawesome-webfont.ttf"})
	{
		EmbeddedFile file = embeddedFile(name);
		ASSERT_NE(file.data, nullptr) << name;
		ASSERT_GT(file.size, 1000) << name;
		// TrueType fonts start with version 1.0
		EXPECT_EQ(file.data[0], 0x00);
		EXPECT_EQ(file.data[1], 0x01);
		EXPECT_EQ(file.data[2], 0x00);
		EXPECT_EQ(file.data[3], 0x00);
	}

	EXPECT_EQ(embeddedFile("missing.ttf").data, nullptr);
}
