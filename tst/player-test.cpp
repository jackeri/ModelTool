#include "gtest/gtest.h"
#include "filesystem.h"
#include "model.h"
#include "model_md3.h"
#include "morph_model.h"
#include "player.h"
#include "posed_model.h"
#include "skin.h"
#include "state.h"
#include <filesystem>

using namespace mt;

TEST(SkinTest, Parse)
{
	auto skin = model::parseSkin("tag_head,\n"
								 "tag_weapon,\n"
								 "u_torso,models/players/robot/robot.tga\r\n"
								 "  U_LArm , \"models/players/robot/arm.tga\" // comment\n"
								 "u_body \"models/players/temperate/body.tga\"\n"
								 "md3_hat, \"models/players/temperate/acc/helmet.md3\"\n"
								 "// u_skipped, nothing\n");
	EXPECT_EQ(skin.surfaces.size(), 3);
	EXPECT_EQ(skin.shader("u_torso"), "models/players/robot/robot.tga");
	EXPECT_EQ(skin.shader("U_TORSO"), "models/players/robot/robot.tga");
	EXPECT_EQ(skin.shader("u_larm"), "models/players/robot/arm.tga");
	EXPECT_EQ(skin.shader("u_body"), "models/players/temperate/body.tga");
	EXPECT_EQ(skin.shader("tag_head"), "");
	EXPECT_EQ(skin.shader("u_skipped"), "");
	ASSERT_EQ(skin.accessories.size(), 1);
	EXPECT_EQ(skin.accessories.at("md3_hat"), "models/players/temperate/acc/helmet.md3");

	io::FileSystem files;
	files.addPath("data");
	auto lower = model::loadSkin(files.loadFile("models/players/robot/lower_red.skin"));
	EXPECT_EQ(lower.shader("l_legs"), "models/players/robot/red.tga");
	EXPECT_THROW(model::loadSkin(files.loadFile("models/players/robot/lower_none.skin")), mt_exception);
}

TEST(PlayerTest, Config)
{
	io::FileSystem files;
	files.addPath("data");
	auto config = model::parsePlayerConfig(model::fileText(files.loadFile("models/players/robot/animation.cfg")));

	EXPECT_EQ(config.sex, "n");
	EXPECT_EQ(config.footsteps, "mech");
	EXPECT_FLOAT_EQ(config.headOffset.x, -2);
	ASSERT_EQ(config.animations.size(), 25);
	EXPECT_EQ(config.animations[0].name, "BOTH_DEATH1");
	EXPECT_EQ(config.animations[24].name, "LEGS_TURN");

	// LEGS_WALKCR is 65 in the file, the 35 torso only frames (30 - 64) are not in lower.md3
	EXPECT_EQ(config.animations[13].name, "LEGS_WALKCR");
	EXPECT_EQ(config.animations[13].firstFrame, 30);
	EXPECT_EQ(config.animations[13].loopFrames, 8);
	EXPECT_EQ(config.animations[11].name, "TORSO_STAND");
	EXPECT_EQ(config.animations[11].firstFrame, 63);

	auto legs = config.legsAnimations();
	auto torso = config.torsoAnimations();
	EXPECT_EQ(legs.size(), 6 + 12);
	EXPECT_EQ(torso.size(), 6 + 7);

	// The sequences fit the models' frames
	std::unique_ptr<Model> lower(model::io::loadModel(files.loadFile("models/players/robot/lower.md3")));
	std::unique_ptr<Model> upper(model::io::loadModel(files.loadFile("models/players/robot/upper.md3")));
	for (const auto &animation : legs)
	{
		EXPECT_LT(std::max(animation.startFrame, animation.lastFrame()), lower->numFrames()) << animation.name;
	}
	for (const auto &animation : torso)
	{
		EXPECT_LT(std::max(animation.startFrame, animation.lastFrame()), upper->numFrames()) << animation.name;
	}
	EXPECT_EQ(legs.back().lastFrame(), lower->numFrames() - 1);
	EXPECT_EQ(torso.back().lastFrame(), upper->numFrames() - 1);
}

TEST(PlayerTest, ConfigVariants)
{
	std::string lines;
	for (int i = 0; i < 25; i++)
	{
		lines += std::to_string(i * 10) + " " + (i == 1 ? "-5" : "5") + " 0 0\n";
	}

	auto config = model::parsePlayerConfig("sex f\nfootsteps boot\nfixedlegs\n" + lines);
	EXPECT_EQ(config.sex, "f");
	EXPECT_EQ(config.footsteps, "boot");
	EXPECT_TRUE(config.fixedLegs);
	EXPECT_TRUE(config.animations[1].reversed);
	EXPECT_EQ(config.animations[1].numFrames, 5);
	EXPECT_EQ(config.animations[1].fps, 1);
	auto reversed = config.animations[1].toAnimation();
	EXPECT_EQ(reversed.startFrame, 14);
	EXPECT_EQ(reversed.lastFrame(), 10);

	// Team Arena extras
	auto extended = model::parsePlayerConfig(lines + "0 1 0 15\n0 1 0 15\n");
	ASSERT_EQ(extended.animations.size(), 27);
	EXPECT_EQ(extended.animations[26].name, "TORSO_GUARDBASE");

	EXPECT_THROW(model::parsePlayerConfig("sex m\n0 30 0 25\n"), mt_exception);
	EXPECT_THROW(model::parsePlayerConfig("wings 2\n" + lines), mt_exception);
	EXPECT_THROW(model::parsePlayerConfig("0 x 0 25\n" + lines), mt_exception);
}

TEST(PlayerTest, Files)
{
	io::FileSystem files;
	files.addPath("data");

	model::PlayerFiles player;
	ASSERT_TRUE(model::findPlayerFiles(files, "models/players/robot/animation.cfg", player));
	EXPECT_EQ(player.folder, "models/players/robot");
	EXPECT_EQ(player.upper, "models/players/robot/upper.md3");
	ASSERT_EQ(player.skins.size(), 4);
	EXPECT_EQ(player.skins[0], "default");
	EXPECT_EQ(player.skins[1], "blue");
	EXPECT_EQ(player.skinPath("head", "red"), "models/players/robot/head_red.skin");

	EXPECT_TRUE(model::findPlayerFiles(files, "models/players/robot/head.md3", player));
	EXPECT_FALSE(model::findPlayerFiles(files, "models/players/robot/head_1.md3", player));
	EXPECT_FALSE(model::findPlayerFiles(files, "models/md3/tube.md3", player));

	// From the disk
	io::FileSystem disk;
	ASSERT_TRUE(model::findPlayerFiles(disk, std::filesystem::absolute("data/models/players/robot/lower.md3").string(), player));
	EXPECT_EQ(player.skins.size(), 4);
}

TEST(PlayerTest, Attachment)
{
	io::FileSystem files;
	files.addPath("data");
	std::unique_ptr<Model> lower(model::io::loadModel(files.loadFile("models/players/robot/lower.md3")));
	std::unique_ptr<Model> upper(model::io::loadModel(files.loadFile("models/players/robot/upper.md3")));

	model::PoseOptions options;
	options.frame = 100;
	auto posedLower = model::poseModel(*lower, options);
	glm::mat4 torso{1.0f};
	ASSERT_TRUE(model::tagMatrix(posedLower, "TAG_TORSO", torso));
	glm::mat4 missing{1.0f};
	EXPECT_FALSE(model::tagMatrix(posedLower, "tag_nothing", missing));

	auto *morph = dynamic_cast<model::morph::MorphModel *>(lower.get());
	ASSERT_NE(morph, nullptr);
	const Point &tag = morph->tags[0].name == "tag_torso" ? morph->tags[0].frames[100] : morph->tags[1].frames[100];
	glm::vec3 origin = torso * glm::vec4(0, 0, 0, 1);
	EXPECT_NEAR(origin.x, tag.location.x, 1e-4f);
	EXPECT_NEAR(origin.z, tag.location.z, 1e-4f);

	// The upper's tag_head goes along with the torso
	auto posedUpper = model::poseModel(*upper, {});
	glm::mat4 head{1.0f};
	ASSERT_TRUE(model::tagMatrix(posedUpper, "tag_head", head));
	glm::vec3 headOnLower = torso * head * glm::vec4(0, 0, 0, 1);
	EXPECT_GT(headOnLower.z, origin.z);

	// Skins replace the materials of the meshes they name
	model::applySkin(posedLower, model::parseSkin("l_legs,models/players/robot/red.tga\n"));
	EXPECT_EQ(posedLower.meshes[0].material, "models/players/robot/red.tga");
}

TEST(PlayerTest, StateLoadsPlayer)
{
	auto &state = singleton<State>();
	state.clear();
	state.filesystem.clear();
	ASSERT_TRUE(state.filesystem.addPath("data"));

	state.loadModel("models/players/robot/animation.cfg");
	ASSERT_TRUE(state.isPlayer);
	ASSERT_EQ(state.attachments.size(), 2);
	EXPECT_EQ(state.attachments[0].tag, "tag_torso");
	EXPECT_EQ(state.attachments[1].parent, 0);
	EXPECT_EQ(state.attachments[1].tag, "tag_head");
	EXPECT_EQ(state.playerSkin, "default");
	EXPECT_EQ(state.skin.shader("l_legs"), "models/players/robot/robot.tga");
	EXPECT_EQ(state.attachments[1].skin.shader("h_head"), "models/players/robot/robot_h.tga");

	// Idle sequences, played together
	EXPECT_EQ(state.model->currentFrame, 136 - 35);			 // LEGS_IDLE
	EXPECT_EQ(state.attachments[0].model->currentFrame, 63); // TORSO_STAND
	auto &legs = state.model->animations;
	auto walk = std::find_if(legs.begin(), legs.end(), [](const auto &animation) { return animation.name == "LEGS_WALKCR"; });
	ASSERT_NE(walk, legs.end());
	state.playSequence(-1, *walk);
	state.playback.play();
	state.update(0.1);
	EXPECT_TRUE(state.attachments[0].playback.isPlaying());
	EXPECT_EQ(state.model->currentFrame, 32);

	state.setPlayerSkin("red");
	EXPECT_EQ(state.skin.shader("l_legs"), "models/players/robot/red.tga");

	// Attach, then remove the upper and everything on it
	state.attachModel("models/md3/tube.md3", 1, "tag_head");
	EXPECT_EQ(state.tagNames(0), std::vector<std::string>({"tag_head", "tag_weapon", "tag_torso"}));
	state.removeAttachment(0);
	EXPECT_TRUE(state.attachments.empty());
	EXPECT_FALSE(state.isPlayer);

	EXPECT_THROW(state.loadModel("models/md5/animation.cfg"), mt_exception);

	state.clear();
	state.filesystem.clear();
}

TEST(PlayerTest, AttachToBone)
{
	auto &state = singleton<State>();
	state.clear();
	state.filesystem.clear();
	ASSERT_TRUE(state.filesystem.addPath("data"));

	state.loadModel("models/mdm/mannequin.mdm");
	state.loadAnimation("models/mdm/mannequin.mdx");

	// Tags first, then the bones
	auto points = state.attachPoints(-1);
	ASSERT_FALSE(points.empty());
	EXPECT_EQ(points.front(), "tag_head");
	EXPECT_NE(std::find(points.begin(), points.end(), "Bip01 Head"), points.end());

	state.attachModel("models/players/robot/head.md3", -1, "Bip01 Head");
	ASSERT_EQ(state.attachments.size(), 1);

	// The posed bone gives the attachment's place
	model::PoseOptions options;
	auto posed = model::poseModel(*state.model, options);
	glm::mat4 bone{1.0f};
	ASSERT_TRUE(model::tagMatrix(posed, "bip01 head", bone));
	auto head
		= std::find_if(posed.bonePoints.begin(), posed.bonePoints.end(), [](const auto &point) { return point.name == "Bip01 Head"; });
	ASSERT_NE(head, posed.bonePoints.end());
	glm::vec3 origin = bone * glm::vec4(0, 0, 0, 1);
	EXPECT_NEAR(glm::distance(origin, head->point.location), 0, 1e-4f);

	// An attached model takes attachments on its own tags
	EXPECT_EQ(state.attachPoints(0), std::vector<std::string>({"tag_head"}));
	state.attachModel("models/md3/tube.md3", 0, "tag_head");
	EXPECT_EQ(state.attachments[1].parent, 0);

	state.clear();
	state.filesystem.clear();
}

TEST(PlayerTest, BakeSkin)
{
	io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	std::unique_ptr<Model> loaded(model::io::loadModel(files.loadFile("models/players/robot/head.md3")));
	auto &head = dynamic_cast<model::morph::MorphModel &>(*loaded);
	model::loadMorphLods(head, files, "models/players/robot/head.md3");
	ASSERT_EQ(head.lods.size(), 2);

	// The skin's shader goes into the base mesh and every level of detail
	const auto skin = model::loadSkin(files.loadFile("models/players/robot/head_red.skin"));
	EXPECT_EQ(model::bakeSkin(head, skin), 3);
	EXPECT_EQ(head.meshes[0].materialName, "models/players/robot/red_h.tga");
	EXPECT_EQ(head.lods[1][0].materialName, "models/players/robot/red_h.tga");
	// Already baked, and a skin that names no mesh changes nothing
	EXPECT_EQ(model::bakeSkin(head, skin), 0);
	EXPECT_EQ(model::bakeSkin(head, model::parseSkin("other,x.tga\n")), 0);
	EXPECT_EQ(head.meshes[0].materialName, "models/players/robot/red_h.tga");
}

TEST(PlayerTest, StateBakesSkins)
{
	auto &state = singleton<State>();
	state.clear();
	state.filesystem.clear();
	ASSERT_TRUE(state.filesystem.addPath("data"));

	// A player model: lower with its skin, upper and head attached with theirs
	state.loadModel("models/players/robot/animation.cfg");
	state.setPlayerSkin("blue");
	ASSERT_TRUE(state.hasSkins());
	EXPECT_EQ(state.bakeSkins(), 7); // the meshes of the three parts and of their levels of detail
	EXPECT_FALSE(state.hasSkins());
	EXPECT_EQ(state.model->skeletalModel(), false);
	auto &lower = dynamic_cast<model::morph::MorphModel &>(*state.model);
	EXPECT_EQ(lower.meshes[0].materialName, "models/players/robot/blue.tga");

	// Any model can get a skin
	state.loadModel("models/players/robot/head.md3");
	EXPECT_FALSE(state.hasSkins());
	state.loadModelSkin("models/players/robot/head_stripe.skin");
	EXPECT_TRUE(state.hasSkins());
	EXPECT_THROW(state.loadModelSkin("models/players/robot/missing.skin"), mt_exception);

	state.clear();
	state.filesystem.clear();
}
