#include "gtest/gtest.h"
#include "et_character.h"
#include "filesystem.h"
#include "model.h"
#include "posed_model.h"
#include "skeletal_model.h"
#include "state.h"

#include <memory>
#include <string>
#include <vector>

using namespace mt;

namespace {
	std::string text(io::FileSystem &files, const std::string &path)
	{
		auto file = files.loadFile(path);
		if (!file)
		{
			throw mt_ex("File not found: " + path);
		}
		return model::fileText(file);
	}
}

TEST(EtCharacterTest, Parse)
{
	// Keys ignore the case like in the game
	auto character = model::parseCharacter("characterDef\n{\n\tskin \"cvops\"\n\tMESH \"models/players/axis/cvops/body.mdm\"\n"
										   "\tanimationGroup \"animations/human_base.anim\"\n\tanimationscript \"x.script\"\n"
										   "\tundressedCorpseModel \"models/common/naked.mdm\"\n\tundressedCorpseSkin \"axis_cvops\"\n"
										   "\thudhead \"models/players/hud/head.md3\"\n\thudheadanims \"animations/heads/base.anim\"\n"
										   "\thudheadskin \"models/players/hud/axis_cvops.skin\" // comment\n}\n");
	EXPECT_EQ(character.mesh, "models/players/axis/cvops/body.mdm");
	EXPECT_EQ(character.animationGroup, "animations/human_base.anim");
	EXPECT_EQ(character.hudHeadSkin, "models/players/hud/axis_cvops.skin");

	// The game's skin file names
	EXPECT_EQ(character.skinPath(), "models/players/axis/cvops/body_cvops.skin");
	EXPECT_EQ(character.corpseSkinPath(), "models/common/naked_axis_cvops.skin");
	character.undressedCorpseModel.clear();
	EXPECT_EQ(character.corpseSkinPath(), "");

	EXPECT_THROW(model::parseCharacter("characterDef { mesh \"a.mdm\" wings \"2\" }"), mt_exception);
	EXPECT_THROW(model::parseCharacter("characterDef { skin \"a\" }"), mt_exception);
	EXPECT_THROW(model::parseCharacter("characterDef { mesh \"a.mdm\""), mt_exception);
	EXPECT_THROW(model::parseCharacter("character { mesh \"a.mdm\" }"), mt_exception);
	EXPECT_THROW(model::parseCharacter("characterDef { mesh\n}"), mt_exception);
}

TEST(EtCharacterTest, Accessories)
{
	auto skin = model::parseSkin("md3_hat, \"models/hat.md3\"\nmd3_beltl, \"models/l.md3\"\nmd3_beltr, \"models/r.md3\"\n"
								 "md3_weapon2, \"models/w.md3\"\nmd3_wings, \"models/wings.md3\"\n//md3_back, \"models/back.md3\"\n"
								 "u_body, \"body.tga\"\n");
	auto accessories = model::characterAccessories(skin);

	// The game's order: body accessories, then head ones. Unknown md3_ lines are not used
	ASSERT_EQ(accessories.size(), 4);
	EXPECT_EQ(accessories[0].type, "md3_beltr");
	EXPECT_EQ(accessories[0].tag, "tag_bright"); // beltr goes on tag_bright like in the game
	EXPECT_FALSE(accessories[0].onHead);
	EXPECT_EQ(accessories[1].tag, "tag_bleft");
	EXPECT_EQ(accessories[2].tag, "tag_weapon2");
	EXPECT_EQ(accessories[3].type, "md3_hat");
	EXPECT_TRUE(accessories[3].onHead);
	EXPECT_EQ(accessories[3].tag, "tag_mouth");
	EXPECT_EQ(accessories[3].skinPath("medic"), "models/hat_medic.skin");
}

TEST(EtCharacterTest, AnimationGroup)
{
	io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	auto include = [&](const std::string &path) { return text(files, path); };

	auto group = model::parseAnimationGroup(text(files, "animations/mannequin.anim"), include);
	ASSERT_EQ(group.size(), 2);
	EXPECT_EQ(group[0].mdx, "models/mdm/mannequin.mdx");
	EXPECT_EQ(group[0].animations.size(), 7); // from the #include
	EXPECT_EQ(group[1].animations.size(), 2); // inline
	EXPECT_EQ(group[1].animations[0].name, "salute");
	EXPECT_EQ(group[1].animations[0].transition, 100);

	// The frames of both MDX files one after another, the sequences move along
	std::unique_ptr<Model> loaded(model::io::loadModel(files.loadFile("models/mdm/mannequin.mdm")));
	auto &body = dynamic_cast<model::skeletal::SkeletalModel &>(*loaded);
	auto load = [&](const std::string &path) { return files.loadFile(path); };
	model::loadAnimationGroup(body, group, load);
	EXPECT_EQ(body.numFrames(), 116 + 20);
	EXPECT_EQ(body.bounds.size(), 136);
	ASSERT_EQ(body.animations.size(), 9);
	EXPECT_EQ(body.animations[3].name, "wave");
	EXPECT_EQ(body.animations[3].startFrame, 60);
	EXPECT_EQ(body.animations[7].name, "salute");
	EXPECT_EQ(body.animations[7].startFrame, 116);
	EXPECT_EQ(body.animations[8].lastFrame(), 135);

	// The salute MDX holds the wave frames: the same pose
	model::PoseOptions wave, salute;
	wave.frame = 70;
	salute.frame = 126;
	auto wavePose = model::poseModel(body, wave);
	auto salutePose = model::poseModel(body, salute);
	ASSERT_EQ(wavePose.bonePoints.size(), salutePose.bonePoints.size());
	for (size_t i = 0; i < wavePose.bonePoints.size(); i++)
	{
		EXPECT_LT(glm::distance(wavePose.bonePoints[i].point.location, salutePose.bonePoints[i].point.location), 0.01f)
			<< wavePose.bonePoints[i].name;
	}

	// A sequence outside its file or a missing file leaves the model as it was
	auto broken = group;
	broken[1].animations[0].numFrames = 30;
	EXPECT_THROW(model::loadAnimationGroup(body, broken, load), mt_exception);
	EXPECT_EQ(body.numFrames(), 136);
	EXPECT_EQ(body.animations.size(), 9);
	broken = group;
	broken[1].mdx = "animations/missing.mdx";
	EXPECT_THROW(model::loadAnimationGroup(body, broken, load), mt_exception);
	EXPECT_EQ(body.numFrames(), 136);

	EXPECT_THROW(model::parseAnimationGroup("animgroup { animfile \"a.mdx\" { idle 0 x 0 20 0 0 0 } }", include), mt_exception);
	EXPECT_THROW(model::parseAnimationGroup("animgroup { mesh \"a.mdx\" }", include), mt_exception);
	EXPECT_THROW(model::parseAnimationGroup("animgroup { animfile \"a.mdx\" { #include \"missing.aninc\" } }", include), mt_exception);
	EXPECT_THROW(model::parseAnimationGroup("animgroup { animfile \"a.mdx\" { idle 0 1 0 20 0 0 0 ", include), mt_exception);
}

TEST(EtCharacterTest, HeadAnimations)
{
	io::FileSystem files;
	ASSERT_TRUE(files.addPath("data"));
	auto animations = model::parseHeadAnimations(text(files, "animations/mannequin/head.anim"));
	ASSERT_EQ(animations.size(), 16);
	EXPECT_EQ(animations[0].name, "HD_IDLE1");
	EXPECT_TRUE(animations[0].loop);
	EXPECT_EQ(animations[15].name, "HD_PAIN");
	EXPECT_EQ(animations[15].fps, 15);
	EXPECT_FALSE(animations[15].loop);

	// The game reads the fps as a float, 0 means 1. Lines after the 16th are not used
	std::string lines;
	for (int i = 0; i < 18; i++)
	{
		lines += std::to_string(i) + " 2 " + (i == 1 ? "0" : "7.6") + " 0\n";
	}
	animations = model::parseHeadAnimations(lines);
	EXPECT_EQ(animations[1].fps, 1);
	EXPECT_EQ(animations[2].fps, 8);

	EXPECT_THROW(model::parseHeadAnimations("0 10 10 0\n"), mt_exception);
	EXPECT_THROW(model::parseHeadAnimations("x 10 10 0\n" + lines), mt_exception);
}

TEST(EtCharacterTest, StateLoadsCharacter)
{
	auto &state = singleton<State>();
	state.clear();
	state.filesystem.clear();
	ASSERT_TRUE(state.filesystem.addPath("data"));

	state.loadModel("characters/mannequin/soldier.char");
	ASSERT_TRUE(state.isCharacter);
	EXPECT_EQ(state.model->numFrames(), 136);
	EXPECT_EQ(state.skin.shader("u_body"), "models/players/mannequin/soldier.tga");

	// Head on tag_head, then the accessories in the game's order; the rank badge is left out (unranked)
	ASSERT_EQ(state.attachments.size(), 4);
	EXPECT_EQ(state.attachments[0].name, "head");
	EXPECT_EQ(state.attachments[0].tag, "tag_head");
	EXPECT_EQ(state.attachments[0].skin.shader("h_head"), "models/players/mannequin/face");
	ASSERT_EQ(state.attachments[0].model->animations.size(), 16);
	EXPECT_EQ(state.attachments[0].playback.first(), 0); // HD_IDLE1
	EXPECT_EQ(state.attachments[0].playback.last(), 9);
	EXPECT_EQ(state.attachments[1].name, "md3_beltr");
	EXPECT_EQ(state.attachments[1].tag, "tag_bright");
	EXPECT_EQ(state.attachments[1].parent, -1);
	EXPECT_EQ(state.attachments[2].name, "md3_back");
	EXPECT_EQ(state.attachments[3].name, "md3_hat");
	EXPECT_EQ(state.attachments[3].parent, 0); // on the head
	EXPECT_EQ(state.attachments[3].tag, "tag_mouth");
	EXPECT_TRUE(state.attachments[3].skin.empty()); // no helmet_soldier.skin

	// Starts with the idle sequence
	EXPECT_EQ(state.playback.first(), 0);
	EXPECT_EQ(state.playback.last(), 19);

	// The helmet follows the head's tag_mouth, which follows the body's tag_head
	model::PoseOptions options;
	options.frame = 0;
	auto body = model::poseModel(*state.model, options);
	glm::mat4 headTag{1.0f};
	ASSERT_TRUE(model::tagMatrix(body, "tag_head", headTag));
	auto head = model::poseModel(*state.attachments[0].model, options);
	glm::mat4 mouthTag{1.0f};
	ASSERT_TRUE(model::tagMatrix(head, "tag_mouth", mouthTag));
	glm::vec3 helmet = headTag * mouthTag * glm::vec4(0, 0, 0, 1);
	glm::vec3 headOrigin = headTag * glm::vec4(0, 0, 0, 1);
	EXPECT_NEAR(glm::distance(helmet, headOrigin), glm::length(glm::vec2(3, 2)), 1e-3f);

	// The medic has the optional accessory skin
	state.loadModel("characters/mannequin/medic.char");
	ASSERT_EQ(state.attachments.size(), 3);
	EXPECT_EQ(state.attachments[2].name, "md3_hat");
	EXPECT_EQ(state.attachments[2].skin.shader("helmet"), "models/players/mannequin/helmet_medic");

	// The undressed corpse: corpse skin, no accessories
	state.loadCharacter("characters/mannequin/medic.char", true);
	EXPECT_TRUE(state.characterUndressed);
	EXPECT_EQ(state.skin.shader("u_body"), "models/players/mannequin/naked.tga");
	ASSERT_EQ(state.attachments.size(), 1);
	EXPECT_EQ(state.attachments[0].name, "head");

	// A broken file keeps the loaded character
	EXPECT_THROW(state.loadModel("characters/mannequin/missing.char"), mt_exception);
	EXPECT_TRUE(state.isCharacter);

	state.clear();
	state.filesystem.clear();
}
