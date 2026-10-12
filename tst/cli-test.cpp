#include "gtest/gtest.h"
#include "cli.h"
#include "filesystem.h"
#include "model.h"
#include "morph_model.h"
#include "skeletal_model.h"

#include <filesystem>
#include <sstream>

using namespace mt;

namespace {
	struct Result {
		int code = 0;
		std::string out{};
		std::string err{};
	};

	Result run(const std::vector<std::string> &args)
	{
		std::ostringstream out, err;
		Result result;
		result.code = cli::runCli(args, out, err);
		result.out = out.str();
		result.err = err.str();
		return result;
	}

	bool contains(const std::string &text, const std::string &part)
	{
		return text.find(part) != std::string::npos;
	}
}

TEST(CliTest, Usage)
{
	auto none = run({});
	EXPECT_EQ(none.code, cli::EXIT_USAGE);
	EXPECT_TRUE(contains(none.err, "Usage: mt"));

	auto help = run({"--help"});
	EXPECT_EQ(help.code, cli::EXIT_OK);
	EXPECT_TRUE(contains(help.out, "convert <model> <output>"));

	EXPECT_EQ(run({"--version"}).code, cli::EXIT_OK);
	EXPECT_EQ(run({"nothing"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"info", "--what"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"info"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "--frames", "3"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "out.md3", "--scale", "-1"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "out.md3", "--normals", "round"}).code, cli::EXIT_USAGE);
	EXPECT_EQ(run({"list"}).code, cli::EXIT_USAGE);
}

TEST(CliTest, Formats)
{
	auto result = run({"formats"});
	EXPECT_EQ(result.code, cli::EXIT_OK);
	EXPECT_TRUE(contains(result.out, ".md3"));
	EXPECT_TRUE(contains(result.out, ".md5anim"));
}

TEST(CliTest, Info)
{
	auto morph = run({"info", "data/models/md3/pf.md3"});
	ASSERT_EQ(morph.code, cli::EXIT_OK) << morph.err;
	EXPECT_TRUE(contains(morph.out, "Type: morph"));
	EXPECT_TRUE(contains(morph.out, "w_panzer  vertices 254, triangles 271"));
	EXPECT_TRUE(contains(morph.out, "tag_flash"));

	// From a base folder, with animations
	auto skeletal = run(
		{"--base", "data", "info", "models/mdm/medic.mdm", "--animation", "models/mdm/medic.mdx", "--animation", "models/mdm/medic.aninc"});
	ASSERT_EQ(skeletal.code, cli::EXIT_OK) << skeletal.err;
	EXPECT_TRUE(contains(skeletal.out, "Type: skeletal"));
	EXPECT_TRUE(contains(skeletal.out, "Bones: 53"));
	EXPECT_TRUE(contains(skeletal.out, "tag_torso  bone Bip01 Spine"));
	EXPECT_TRUE(contains(skeletal.out, "Frames: 4887"));
	EXPECT_TRUE(contains(skeletal.out, "death_gut"));
	EXPECT_TRUE(contains(skeletal.out, "Mesh bounds"));

	// Levels of detail next to the model are loaded like in the game
	auto lods = run({"--base", "data", "info", "models/players/grunt/head.md3"});
	ASSERT_EQ(lods.code, cli::EXIT_OK) << lods.err;
	EXPECT_TRUE(contains(lods.out, "Levels of detail: 3"));

	auto missing = run({"info", "data/models/missing.md3"});
	EXPECT_EQ(missing.code, cli::EXIT_FAILED);
	EXPECT_TRUE(contains(missing.err, "File not found"));
}

TEST(CliTest, Convert)
{
	std::filesystem::create_directories("tmp/cli");

	auto result = run({"convert", "data/models/md5/bob_lamp_update_export.md5mesh", "tmp/cli/bob.md3", "tmp/cli/bob.md5anim", "--animation",
					   "data/models/md5/bob_lamp_update_export.md5anim", "--frames", "2:5", "--scale", "2", "--normals", "flat"});
	ASSERT_EQ(result.code, cli::EXIT_OK) << result.err;
	EXPECT_TRUE(contains(result.out, "Wrote tmp/cli/bob.md3"));

	io::FileSystem disk;
	std::unique_ptr<Model> md3(model::io::loadModel(disk.loadFile(std::filesystem::absolute("tmp/cli/bob.md3").string())));
	ASSERT_TRUE(md3);
	EXPECT_EQ(md3->numFrames(), 4);

	// The scale is baked into the written model
	auto original = run({"info", "data/models/md5/bob_lamp_update_export.md5mesh"});
	auto scaled = run({"info", "tmp/cli/bob.md3"});
	EXPECT_TRUE(contains(original.out, "Mesh bounds"));
	EXPECT_NE(original.out.substr(original.out.find("Mesh bounds")), scaled.out.substr(scaled.out.find("Mesh bounds")));

	EXPECT_TRUE(std::filesystem::exists("tmp/cli/bob.md5anim"));

	// Hitboxes in and out
	auto hitboxes = run({"convert", "data/models/mdm/medic.mdm", "--animation", "data/models/mdm/medic.mdx", "--hitboxes",
						 "data/test/test.hitbox", "--save-hitboxes", "tmp/cli/medic.hitboxes", "--optimize"});
	ASSERT_EQ(hitboxes.code, cli::EXIT_OK) << hitboxes.err;
	EXPECT_TRUE(std::filesystem::exists("tmp/cli/medic.hitboxes"));
	auto info = run({"info", "data/models/mdm/medic.mdm", "--hitboxes", "tmp/cli/medic.hitboxes"});
	EXPECT_TRUE(contains(info.out, "Hitboxes: 3"));

	// Errors
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "tmp/cli/pf.xyz"}).code, cli::EXIT_FAILED);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "tmp/cli/pf.md3", "--frames", "0:5"}).code, cli::EXIT_FAILED);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "tmp/cli/pf.md3", "--optimize"}).code, cli::EXIT_FAILED);
	EXPECT_EQ(run({"convert", "data/models/md3/pf.md3", "--save-hitboxes", "tmp/cli/pf.hitboxes"}).code, cli::EXIT_FAILED);

	std::filesystem::remove_all("tmp");
}

TEST(CliTest, List)
{
	auto all = run({"--base", "data", "list"});
	ASSERT_EQ(all.code, cli::EXIT_OK) << all.err;
	EXPECT_TRUE(contains(all.out, "models/md3/pf.md3\n"));
	EXPECT_TRUE(contains(all.out, "models/mdm/medic.mdx\n"));
	EXPECT_FALSE(contains(all.out, ".tga"));

	auto folder = run({"--base", "data", "list", "models/md5/"});
	EXPECT_EQ(folder.out, "models/md5/bob_lamp_update_export.md5mesh\n");

	EXPECT_EQ(run({"--base", "data/missing", "list"}).code, cli::EXIT_FAILED);
}
