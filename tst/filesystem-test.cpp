#include "gtest/gtest.h"
#include "filesystem.h"

using namespace mt;

TEST(FileSystemTest, test1) {
	IO::FileSystem sys;
	sys.addPath("data");

	ASSERT_EQ(sys.findFile("test.cfg"), true);
}