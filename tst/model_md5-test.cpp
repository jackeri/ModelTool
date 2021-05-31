#include "gtest/gtest.h"
#include "ModelMD5.h"
#include "SkeletalModel.h"

using namespace mt;
using namespace mt::model::Skeletal;

TEST(ModelMD5Test, test1)
{
	IO::MTPath path("data");

	std::unique_ptr<SkeletalModel> model;
	ASSERT_NO_THROW(model = std::unique_ptr<SkeletalModel>(dynamic_cast<SkeletalModel *>(model::loadMd5Model(
			path.loadFile("models/md5/bob_lamp_update_export.md5mesh"))));
	);

	// Make sure the right ammount of meshes was parsed
	ASSERT_EQ(model->meshes.size(), 6);

	// Check the first mesh for some information (these are just manually checked from the text data)
	ASSERT_EQ(model->meshes[0].name, "bob_body");
	ASSERT_EQ(model->meshes[0].vertices.size(), 489);
	ASSERT_EQ(model->meshes[0].tris.size(), 628);
	ASSERT_EQ(model->meshes[0].weights.size(), 859);

	// Load animation data into the model
	ASSERT_NO_THROW(model::loadMD5Anim(model.get(), path.loadFile("models/md5/bob_lamp_update_export.md5anim")););

	// We should have loaded 141 frames
	ASSERT_EQ(model->numFrames(), 141);
}