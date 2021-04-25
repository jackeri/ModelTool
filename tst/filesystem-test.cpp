#include "gtest/gtest.h"
#include "filesystem.h"

TEST(FileSystemTest, test1) {
	mt::IO::FileSystem sys;
	sys.addPath("data");

	ASSERT_EQ(sys.findFile("test.cfg"), true);
}