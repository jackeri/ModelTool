#include "gtest/gtest.h"
#include "model_md3.h"
#include "idtech.h"
#include "normals.h"
#include "morph_model.h"
#include "mt_path.h"
#include "filesystem.h"
#include "state.h"

using namespace mt;
using namespace mt::model;

namespace {
	std::unique_ptr<morph::MorphModel> loadMorph(const std::string &path)
	{
		mt::io::MTPath data("data");
		std::unique_ptr<Model> loaded(model::io::loadModel(data.loadFile(path)));
		auto *morph = dynamic_cast<morph::MorphModel *>(loaded.get());
		if (morph)
		{
			loaded.release();
		}
		return std::unique_ptr<morph::MorphModel>(morph);
	}
}

TEST(ModelMD3Test, loadsMD3)
{
	auto model = loadMorph("models/md3/pf.md3");
	ASSERT_TRUE(model);
	ASSERT_TRUE(model->morphModel());
	ASSERT_EQ(model->numFrames(), 1);
	ASSERT_EQ(model->meshes.size(), 1);

	const auto &mesh = model->meshes[0];
	ASSERT_EQ(mesh.name, "w_panzer");
	ASSERT_EQ(mesh.materialName, "models/weapons2/panzerfaust/panzerfast1a.tga");
	ASSERT_EQ(mesh.numVertices(), 254);
	ASSERT_EQ(mesh.tris.size(), 271);
	ASSERT_NEAR(mesh.frames[0][0].pos.x, 11.984f, 1e-3f);
	ASSERT_NEAR(glm::length(mesh.frames[0][0].normal), 1.0f, 1e-4f);

	ASSERT_EQ(model->tags.size(), 1);
	ASSERT_EQ(model->tags[0].name, "tag_flash");
	ASSERT_EQ(model->tags[0].frames.size(), 1);
	ASSERT_NO_THROW(model->validate());
}

TEST(ModelMD3Test, mdcMatchesMD3)
{
	// The same weapon in both formats, the MDC has a single base frame so the vertices are identical
	auto md3 = loadMorph("models/md3/pf.md3");
	auto mdc = loadMorph("models/mdc/pf.mdc");
	ASSERT_TRUE(md3 && mdc);

	ASSERT_EQ(mdc->name, "models/weapons2/panzerfaust/pf.md3");
	ASSERT_EQ(mdc->numFrames(), md3->numFrames());
	ASSERT_EQ(mdc->meshes.size(), md3->meshes.size());

	const auto &a = md3->meshes[0];
	const auto &b = mdc->meshes[0];
	ASSERT_EQ(a.materialName, b.materialName);
	ASSERT_EQ(a.tris, b.tris);
	ASSERT_EQ(a.texCoords, b.texCoords);
	for (size_t v = 0; v < a.numVertices(); v++)
	{
		ASSERT_EQ(a.frames[0][v].pos, b.frames[0][v].pos);
		// The normals were encoded separately, they point in nearly the same direction
		ASSERT_GT(glm::dot(a.frames[0][v].normal, b.frames[0][v].normal), 0.95f);
	}

	// The MDC tag sits at the end of the barrel (the MD3 test file has a rotated tag)
	ASSERT_EQ(mdc->tags[0].name, "tag_flash");
	ASSERT_NEAR(mdc->tags[0].frames[0].location.x, 25.203f, 1e-3f);
}

TEST(ModelMD3Test, normalsMatchCalculatedNormals)
{
	auto model = loadMorph("models/players/grunt/head.md3");
	ASSERT_TRUE(model);

	// The calculated normals use the same triangle winding as the stored normals
	std::vector<glm::vec3> stored;
	for (const auto &vert : model->meshes[0].frames[0])
	{
		stored.push_back(vert.normal);
	}

	calculateNormals(*model);

	float total = 0;
	for (size_t v = 0; v < stored.size(); v++)
	{
		total += glm::dot(stored[v], model->meshes[0].frames[0][v].normal);
	}
	ASSERT_GT(total / float(stored.size()), 0.8f);
}

TEST(ModelMD3Test, levelsOfDetail)
{
	ASSERT_EQ(morphLodPath("models/players/grunt/head.md3", 1), "models/players/grunt/head_1.md3");
	ASSERT_EQ(morphLodPath("models/head.MDC", 2), "models/head_2.MDC");
	ASSERT_EQ(morphLodPath("models.dir/head", 1), "models.dir/head_1");

	auto model = loadMorph("models/players/grunt/head.md3");
	ASSERT_TRUE(model);

	mt::io::MTPath data("data");
	addMorphLod(*model, data.loadFile("models/players/grunt/head_1.md3"));
	addMorphLod(*model, data.loadFile("models/players/grunt/head_2.md3"));

	ASSERT_EQ(model->lodCount(), 3);
	ASSERT_EQ(model->lod(1)[0].name, "h_head_1");
	ASSERT_EQ(model->lod(2)[0].numVertices(), 53);

	// A level with a different frame count is rejected and not added
	model->frames.resize(2);
	ASSERT_THROW(addMorphLod(*model, data.loadFile("models/players/grunt/head_1.md3")), mt_exception);
	ASSERT_EQ(model->lodCount(), 3);
	ASSERT_THROW(addMorphLod(*model, nullptr), mt_exception);
}

TEST(ModelMD3Test, stateLoadsLevelsOfDetail)
{
	auto &state = singleton<State>();
	state.clear();
	state.filesystem.clear();
	ASSERT_TRUE(state.filesystem.addPath("data"));

	state.loadModel("models/players/grunt/head.md3");
	auto *morph = dynamic_cast<morph::MorphModel *>(state.model.get());
	ASSERT_TRUE(morph);
	ASSERT_EQ(morph->lodCount(), 3);

	state.clear();
	state.filesystem.clear();
}

TEST(ModelMD3Test, rejectsInvalidData)
{
	mt::io::MTPath data("data");

	// Truncated file
	auto file = data.loadFile("models/md3/pf.md3");
	file->len = 200;
	ASSERT_THROW(loadMD3(file), mt_exception);

	// Wrong format
	ASSERT_THROW(loadMD3(data.loadFile("models/mdc/pf.mdc")), mt_exception);
	ASSERT_THROW(loadMDC(data.loadFile("models/md3/pf.md3")), mt_exception);

	// Surface offset pointing outside the file
	file = data.loadFile("models/md3/pf.md3");
	int32_t badOffset = 1 << 30;
	std::memcpy(file->data->data() + offsetof(idtech::md3Header_t, ofsSurfaces), &badOffset, sizeof(badOffset));
	ASSERT_THROW(loadMD3(file), mt_exception);
}
