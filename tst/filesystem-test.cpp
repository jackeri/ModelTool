#include "gtest/gtest.h"
#include "filesystem.h"

#include <filesystem>
#include <fstream>

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

TEST(FileSystemTest, caseInsensitive)
{
	// The default ignores the case like the game: loose files, pk3dir folders and pk3 files
	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data"));
	EXPECT_TRUE(sys.isCaseInsensitive());

	EXPECT_TRUE(sys.findFile("Models/MD3/PF.md3"));
	EXPECT_TRUE(sys.findFile("PK3INNER/Pk3File.CFG"));
	EXPECT_TRUE(sys.findFile("Inner/TEST.cfg"));
	auto file = sys.loadFile("MODELS/md3/pf.MD3");
	ASSERT_TRUE(file);
	EXPECT_GT(file->len, 0);
	ASSERT_TRUE(sys.loadFile("pk3inner/PK3FILE.cfg"));

	auto listing = sys.listFolder("MODELS/Md3");
	ASSERT_EQ(listing.size(), 1);
	EXPECT_EQ(listing[0].name, "pf.md3");
	EXPECT_EQ(sys.listFolder("PK3INNER").size(), 1);

	// Exact names only, also on systems where the disk ignores the case (macOS)
	sys.setCaseInsensitive(false);
	EXPECT_FALSE(sys.findFile("Models/MD3/PF.md3"));
	EXPECT_FALSE(sys.findFile("PK3INNER/Pk3File.CFG"));
	EXPECT_FALSE(sys.findFile("Inner/TEST.cfg"));
	EXPECT_FALSE(sys.loadFile("MODELS/md3/pf.MD3"));
	EXPECT_TRUE(sys.listFolder("MODELS/Md3").empty());
	EXPECT_TRUE(sys.findFile("models/md3/pf.md3"));
	EXPECT_TRUE(sys.findFile("pk3inner/pk3file.cfg"));
	EXPECT_EQ(sys.listFolder("models/md3").size(), 1);

	// Sources added later follow the setting
	ASSERT_TRUE(sys.addPath("data/test/nodirs.pk3"));
	EXPECT_FALSE(sys.findFile("README.cfg"));
	sys.setCaseInsensitive(true);
	EXPECT_TRUE(sys.findFile("README.cfg"));
}

TEST(FileSystemTest, caseInsensitiveListingMerges)
{
	// Folders that only differ by case are one folder when the case is ignored
	std::filesystem::create_directories("tmp/case/Models");
	std::filesystem::create_directories("tmp/case/other.pk3dir/models");
	std::ofstream("tmp/case/Models/a.md3") << "a";
	std::ofstream("tmp/case/other.pk3dir/models/b.md3") << "b";

	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("tmp/case"));
	auto root = sys.listFolder("");
	ASSERT_EQ(root.size(), 1);
	EXPECT_EQ(root[0].name, "Models");
	EXPECT_EQ(root[0].sources.size(), 2);
	EXPECT_EQ(sys.listFolder("models").size(), 2);

	sys.setCaseInsensitive(false);
	EXPECT_EQ(sys.listFolder("").size(), 2);

	std::filesystem::remove_all("tmp");
}
