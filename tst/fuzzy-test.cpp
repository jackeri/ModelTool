#include "gtest/gtest.h"
#include "fuzzy.h"
#include "filesystem.h"

using namespace mt;

TEST(FuzzyTest, Match)
{
	FuzzyMatch match;
	ASSERT_TRUE(fuzzyMatch("pfmd3", "models/md3/pf.md3", match));
	EXPECT_EQ(match.positions, std::vector<size_t>({11, 12, 14, 15, 16}));

	// Case is ignored, the characters have to be in order
	EXPECT_TRUE(fuzzyMatch("HEAD", "models/players/grunt/head.md3", match));
	EXPECT_FALSE(fuzzyMatch("3dm", "models/players/grunt/head.md3", match));
	EXPECT_FALSE(fuzzyMatch("headx", "models/players/grunt/head.md3", match));
	EXPECT_FALSE(fuzzyMatch("", "head.md3", match));
	EXPECT_FALSE(fuzzyMatch("   ", "head.md3", match));

	// Every word has to match, in any order
	EXPECT_TRUE(fuzzyMatch("md3 grunt", "models/players/grunt/head.md3", match));
	EXPECT_FALSE(fuzzyMatch("grunt wav", "models/players/grunt/head.md3", match));
}

TEST(FuzzyTest, Ranking)
{
	std::vector<std::string> paths = {
		"models/players/grunt/head_1.md3", "models/players/grunt/head.md3", "textures/heads/lambda.tga",
		"models/players/grunt/lower.md3",  "models/players/sarge/head.md3", "sound/hit.wav",
	};

	// File names that start with the pattern win, shorter paths first on a tie
	auto results = fuzzyFind("head", paths);
	ASSERT_EQ(results.size(), 4);
	EXPECT_EQ(paths[results[0].index], "models/players/grunt/head.md3");
	EXPECT_EQ(paths[results[1].index], "models/players/sarge/head.md3");
	EXPECT_EQ(paths[results[2].index], "models/players/grunt/head_1.md3");
	EXPECT_EQ(paths[results[3].index], "textures/heads/lambda.tga");

	// Consecutive characters and word starts beat scattered ones
	results = fuzzyFind("glow", paths);
	ASSERT_FALSE(results.empty());
	EXPECT_EQ(paths[results[0].index], "models/players/grunt/lower.md3");

	results = fuzzyFind("sarge head", paths);
	ASSERT_FALSE(results.empty());
	EXPECT_EQ(paths[results[0].index], "models/players/sarge/head.md3");

	EXPECT_EQ(fuzzyFind("md3", paths, 2).size(), 2);
	EXPECT_TRUE(fuzzyFind("zzz", paths).empty());
}

TEST(FuzzyTest, AllFiles)
{
	io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data"));
	auto files = sys.listAllFiles();

	auto find = [&](const std::string &path) {
		return std::find_if(files.begin(), files.end(), [&](const auto &file) { return file.path == path; });
	};

	// Loose files, pk3 and pk3dir contents, but not the packages themselves
	ASSERT_NE(find("models/md3/pf.md3"), files.end());
	ASSERT_NE(find("pk3inner/pk3file.cfg"), files.end());
	ASSERT_NE(find("inner/test.cfg"), files.end());
	EXPECT_EQ(find("pack.pk3"), files.end());
	EXPECT_EQ(find("test/nodirs.pk3"), files.end());
	EXPECT_EQ(find("test.pk3dir/inner/test.cfg"), files.end());
	EXPECT_TRUE(tools::endsWith(find("pk3inner/pk3file.cfg")->source, "pack.pk3"));
	EXPECT_TRUE(std::is_sorted(files.begin(), files.end(), [](const auto &a, const auto &b) { return a.path < b.path; }));

	// Each file once, from the source it loads from
	io::FileSystem priority;
	ASSERT_TRUE(priority.addPath("data/test/priority"));
	auto list = priority.listAllFiles();
	ASSERT_EQ(list.size(), 3);
	EXPECT_EQ(list[1].path, "same.txt");
	EXPECT_TRUE(tools::endsWith(list[1].source, "priority"));
	EXPECT_TRUE(tools::endsWith(list[0].source, "b.pk3dir"));
}
