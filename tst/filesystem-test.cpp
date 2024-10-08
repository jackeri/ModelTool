#include "gtest/gtest.h"
#include "filesystem.h"

using namespace mt;

TEST(FileSystemTest, test1)
{
	io::FileSystem sys;

	ASSERT_TRUE(sys.addPath("data"));
	ASSERT_TRUE(sys.hasSources());

	io::FileList list = sys.getFiles("");

	ASSERT_EQ(list->size(), 6);
	ASSERT_EQ(list->at(0).name, "inner");
	ASSERT_TRUE(list->at(0).isDirectory);
	ASSERT_EQ(list->at(2).name, "pk3inner");
	ASSERT_TRUE(list->at(2).isDirectory);

	// These files should be directly in the data folder and are accessed with normal system IO
	ASSERT_TRUE(sys.findFile("test/test.cfg"));
	ASSERT_TRUE(sys.findFile("test/test.script"));

	// This is "emulated" pk3 content and is accessed with normal system IO, just the path is relative
	ASSERT_TRUE(sys.findFile("unpacked.dir.cfg"));
	ASSERT_TRUE(sys.findFile("inner/test.cfg"));

	// This should exist in the pack.pk3 file
	ASSERT_TRUE(sys.findFile("pk3inner/pk3file.cfg"));
}
