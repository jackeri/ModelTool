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

TEST(FileSystemTest, packageWithoutFolderEntries)
{
	// Zip tools often store only the files, the folders come from their paths
	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data/test/nodirs.pk3"));

	auto root = sys.listFolder("");
	ASSERT_EQ(root.size(), 2);
	EXPECT_EQ(root[0].name, "models");
	EXPECT_TRUE(root[0].isDirectory);
	EXPECT_EQ(root[1].name, "readme.cfg");

	auto deep = sys.listFolder("models/deep");
	ASSERT_EQ(deep.size(), 1);
	EXPECT_EQ(deep[0].name, "folder");
	EXPECT_TRUE(deep[0].isDirectory);

	auto files = sys.listFolder("models/deep/folder");
	ASSERT_EQ(files.size(), 1);
	EXPECT_EQ(files[0].name, "file.txt");
	EXPECT_TRUE(sys.findFile("models/deep/folder/file.txt"));
}

TEST(FileSystemTest, virtualFolders)
{
	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data"));

	// The pk3 and pk3dir contents are at the root next to the base folder's own files, the packages are hidden
	auto root = sys.listFolder("");
	auto find = [](const std::vector<io::VirtualEntry> &list, const std::string &name) {
		return std::find_if(list.begin(), list.end(), [&](const auto &entry) { return entry.name == name; });
	};
	ASSERT_NE(find(root, "pk3inner"), root.end());
	ASSERT_NE(find(root, "inner"), root.end());
	ASSERT_NE(find(root, "models"), root.end());
	ASSERT_NE(find(root, "unpacked.dir.cfg"), root.end());
	EXPECT_EQ(find(root, "pack.pk3"), root.end());
	EXPECT_EQ(find(root, "test.pk3dir"), root.end());

	// Folders first, then files, each sorted by name
	EXPECT_TRUE(root.front().isDirectory);
	EXPECT_FALSE(root.back().isDirectory);

	// Sources tell where the entries come from
	auto pk3inner = find(root, "pk3inner");
	ASSERT_EQ(pk3inner->sources.size(), 1);
	EXPECT_TRUE(tools::endsWith(pk3inner->sources[0], "pack.pk3"));
	auto inner = find(root, "inner");
	EXPECT_TRUE(tools::endsWith(inner->sources[0], "test.pk3dir"));

	auto files = sys.listFolder("pk3inner");
	ASSERT_EQ(files.size(), 1);
	EXPECT_EQ(files[0].name, "pk3file.cfg");
	EXPECT_FALSE(files[0].isDirectory);

	EXPECT_TRUE(sys.listFolder("does/not/exist").empty());
}

TEST(FileSystemTest, searchOrder)
{
	// Loose files first, then the pk3dir folders, then the pk3 files; later package names win (zz.pk3 over aa.pk3)
	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data/test/priority"));

	auto text = [&](const std::string &name) {
		auto file = sys.loadFile(name);
		return file ? std::string(reinterpret_cast<const char *>(file->data->data()), file->len) : std::string();
	};
	EXPECT_EQ(text("same.txt"), "loose");
	EXPECT_EQ(text("packed.txt"), "b.pk3dir");
	EXPECT_EQ(text("zip.txt"), "zz.pk3");

	// The listing is alphabetical, the sources are in the search order
	auto root = sys.listFolder("");
	ASSERT_EQ(root.size(), 3);
	EXPECT_EQ(root[0].name, "packed.txt");
	EXPECT_EQ(root[1].name, "same.txt");
	EXPECT_EQ(root[2].name, "zip.txt");

	const auto &sources = root[1].sources;
	ASSERT_EQ(sources.size(), 4);
	EXPECT_TRUE(tools::endsWith(sources[0], "priority"));
	EXPECT_TRUE(tools::endsWith(sources[1], "a.pk3dir"));
	EXPECT_TRUE(tools::endsWith(sources[2], "zz.pk3"));
	EXPECT_TRUE(tools::endsWith(sources[3], "aa.pk3"));
}
