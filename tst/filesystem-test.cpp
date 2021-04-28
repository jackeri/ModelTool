#include "gtest/gtest.h"
#include "filesystem.h"

using namespace mt;

TEST(FileSystemTest, test1) {
	IO::FileSystem sys;
	sys.addPath("data");

	// These files should be directly in the data folder and are accessed with normal system IO
	ASSERT_TRUE(sys.findFile("test.cfg"));
	ASSERT_TRUE(sys.findFile("test.script"));

	// This is "emulated" pk3 content and is accessed with normal system IO, just the path is relative
	ASSERT_TRUE(sys.findFile("unpacked.dir.cfg"));
	ASSERT_TRUE(sys.findFile("inner/test.cfg"));

	// This should exist in the pack.pk3 file
	ASSERT_TRUE(sys.findFile("pk3inner/pk3file.cfg"));
}