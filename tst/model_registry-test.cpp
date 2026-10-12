#include "gtest/gtest.h"
#include "model.h"
#include "morph_model.h"
#include "mt_path.h"
#include "filesystem.h"

#include <filesystem>

using namespace mt;
using namespace mt::model;

TEST(ModelRegistryTest, normalizeExtension)
{
	ASSERT_EQ(model::io::normalizeExtension("md3"), ".md3");
	ASSERT_EQ(model::io::normalizeExtension(".MD3"), ".md3");
	ASSERT_EQ(model::io::normalizeExtension("models/players/head.MDC"), ".mdc");
	ASSERT_EQ(model::io::normalizeExtension("medic.mdm"), ".mdm");
	ASSERT_EQ(model::io::normalizeExtension(""), "");
}

TEST(ModelRegistryTest, builtinFormats)
{
	auto models = model::io::modelExtensions();
	ASSERT_NE(std::find(models.begin(), models.end(), ".md5mesh"), models.end());

	auto animations = model::io::animationExtensions();
	ASSERT_NE(std::find(animations.begin(), animations.end(), ".md5anim"), animations.end());
}

TEST(ModelRegistryTest, loadByExtension)
{
	mt::io::MTPath path("data");

	auto file = path.loadFile("models/md5/bob_lamp_update_export.md5mesh");
	std::unique_ptr<Model> loaded(model::io::loadModel(file));
	ASSERT_TRUE(loaded);
	ASSERT_TRUE(loaded->skeletalModel());

	// The extension is case insensitive
	file->ext = ".MD5MESH";
	std::unique_ptr<Model> upper(model::io::loadModel(file));
	ASSERT_TRUE(upper);

	ASSERT_NO_THROW(model::io::loadAnimation(loaded.get(), path.loadFile("models/md5/bob_lamp_update_export.md5anim")));
	ASSERT_GT(loaded->numFrames(), 0);
}

TEST(ModelRegistryTest, errors)
{
	mt::io::MTPath path("data");

	ASSERT_THROW(model::io::loadModel(nullptr), mt_exception);
	ASSERT_THROW(model::io::loadModel(path.loadFile("test/test.cfg")), mt_exception);
	ASSERT_THROW(model::io::loadAnimation(nullptr, path.loadFile("models/md5/bob_lamp_update_export.md5anim")), mt_exception);
	ASSERT_THROW(model::io::writeModel(morph::MorphModel{}, "tmp/out.unknown"), mt_exception);

	// MD5 animations need a skeletal model
	morph::MorphModel morph;
	ASSERT_THROW(model::io::loadAnimation(&morph, path.loadFile("models/md5/bob_lamp_update_export.md5anim")), mt_exception);

	// Invalid data throws instead of returning an empty model
	auto file = path.loadFile("test/test.cfg");
	file->ext = ".md5mesh";
	ASSERT_THROW(model::io::loadModel(file), mt_exception);
}

TEST(ModelRegistryTest, registerFormats)
{
	model::io::registerModelLoader("TESTFMT", [](const Ref<mt::io::MTFile> &) -> Model * { return new morph::MorphModel; });

	// static, the registry keeps the writer after the test
	static std::string written;
	model::io::registerModelWriter(".testfmt", [](const Model &, const std::string &path) { written = path; });

	auto extensions = model::io::modelExtensions();
	ASSERT_NE(std::find(extensions.begin(), extensions.end(), ".testfmt"), extensions.end());

	auto file = make_ref<mt::io::MTFile>();
	file->name = "some/model.TestFmt";
	std::unique_ptr<Model> loaded(model::io::loadModel(file));
	ASSERT_TRUE(loaded && loaded->morphModel());

	model::io::writeModel(*loaded, "out/model.testfmt");
	ASSERT_EQ(written, "out/model.testfmt");
}

TEST(ModelRegistryTest, packageFilesHaveExtensionAndTerminator)
{
	mt::io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data"));

	auto file = sys.loadFile("pk3inner/pk3file.cfg");
	ASSERT_TRUE(file);
	ASSERT_EQ(file->ext, ".cfg");
	ASSERT_EQ(file->data->size(), file->len + 1);
	ASSERT_EQ(file->data->back(), 0);
	ASSERT_EQ(file->string().size(), file->len);
}

TEST(ModelRegistryTest, fileSystemFindsAbsolutePaths)
{
	mt::io::FileSystem sys;
	ASSERT_TRUE(sys.addPath("data"));

	// Models opened from outside the base folders use absolute paths, their textures are found next to them
	std::string absolute = std::filesystem::absolute("data/models/md5/bob_body.tga").string();
	ASSERT_TRUE(sys.findFile(absolute));
	ASSERT_TRUE(sys.loadFile(absolute));

	// Relative paths only come from the sources
	ASSERT_FALSE(sys.findFile("data/models/md5/bob_body.tga"));
	ASSERT_FALSE(sys.findFile(std::filesystem::absolute("data/models/md5/missing.tga").string()));
}
