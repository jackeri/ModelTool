#include "et_character.h"
#include "model_mdx.h"
#include "model_scripts.h"
#include "script_stream.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace mt::model {

	namespace {
		/*! Path without the file extension (the game's COM_StripExtension) */
		std::string stripExtension(const std::string &path)
		{
			const size_t dot = path.find_last_of('.');
			const size_t slash = path.find_last_of("/\\");
			if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
			{
				return path;
			}
			return path.substr(0, dot);
		}

		int parseNumber(ScriptStream &stream, const std::string &field)
		{
			const std::string token = stream.token();
			try
			{
				size_t used = 0;
				int value = std::stoi(token, &used);
				if (used == token.size())
				{
					return value;
				}
			}
			catch (const std::exception &)
			{}
			throw mt_ex("Invalid " + field + ": '" + token + "' (line " + std::to_string(stream.line() + 1) + ")");
		}

		void expect(ScriptStream &stream, const std::string &expected)
		{
			const std::string token = stream.token();
			if (tools::lowercase(token) != tools::lowercase(expected))
			{
				throw mt_ex("Expected '" + expected + "' but found '" + token + "' (line " + std::to_string(stream.line() + 1) + ")");
			}
		}
	}

	std::string CharacterDef::skinPath() const
	{
		return stripExtension(mesh) + "_" + skin + ".skin";
	}

	std::string CharacterDef::corpseSkinPath() const
	{
		return undressedCorpseModel.empty() ? std::string() : stripExtension(undressedCorpseModel) + "_" + undressedCorpseSkin + ".skin";
	}

	CharacterDef parseCharacter(const std::string &text)
	{
		ScriptStream stream(text);
		expect(stream, "characterDef");
		expect(stream, "{");

		CharacterDef character;
		const std::array<std::pair<const char *, std::string CharacterDef::*>, 9> keys = {{
			{"skin", &CharacterDef::skin},
			{"mesh", &CharacterDef::mesh},
			{"animationgroup", &CharacterDef::animationGroup},
			{"animationscript", &CharacterDef::animationScript},
			{"undressedcorpsemodel", &CharacterDef::undressedCorpseModel},
			{"undressedcorpseskin", &CharacterDef::undressedCorpseSkin},
			{"hudhead", &CharacterDef::hudHead},
			{"hudheadanims", &CharacterDef::hudHeadAnims},
			{"hudheadskin", &CharacterDef::hudHeadSkin},
		}};

		while (true)
		{
			if (!stream.hasNext())
			{
				throw mt_ex("The character file is missing '}'");
			}

			const std::string key = stream.token();
			if (key == "}")
			{
				break;
			}

			const std::string lower = tools::lowercase(key);
			auto found = std::find_if(keys.begin(), keys.end(), [&](const auto &entry) { return lower == entry.first; });
			if (found == keys.end())
			{
				throw mt_ex("Unknown character key '" + key + "' (line " + std::to_string(stream.line() + 1) + ")");
			}

			// The value is on the same line
			const std::string value = stream.token(false);
			if (value.empty() || value == "}")
			{
				throw mt_ex("Missing value for character key '" + key + "'");
			}
			character.*(found->second) = value;
		}

		if (character.mesh.empty())
		{
			throw mt_ex("The character has no mesh");
		}

		return character;
	}

	std::string Accessory::skinPath(const std::string &skin) const
	{
		return stripExtension(model) + "_" + skin + ".skin";
	}

	std::vector<Accessory> characterAccessories(const Skin &skin)
	{
		// cg_character.c and cg_players.c: note that beltr goes on tag_bright and beltl on tag_bleft
		struct Rule {
			const char *type;
			bool onHead;
			const char *tag;
		};
		static const std::array<Rule, 10> rules = {{
			{"md3_beltr", false, "tag_bright"},
			{"md3_beltl", false, "tag_bleft"},
			{"md3_belt", false, "tag_ubelt"},
			{"md3_back", false, "tag_back"},
			{"md3_weapon", false, "tag_weapon"},
			{"md3_weapon2", false, "tag_weapon2"},
			{"md3_hat", true, "tag_mouth"},
			{"md3_rank", true, "tag_mouth"},
			{"md3_hat2", true, "tag_mouth"},
			{"md3_hat3", true, "tag_mouth"},
		}};

		std::vector<Accessory> accessories;
		for (const auto &rule : rules)
		{
			auto found = skin.accessories.find(rule.type);
			if (found != skin.accessories.end() && !found->second.empty())
			{
				accessories.push_back({rule.type, found->second, rule.onHead, rule.tag});
			}
		}
		return accessories;
	}

	std::vector<AnimationFile> parseAnimationGroup(const std::string &text, const std::function<std::string(const std::string &)> &include)
	{
		ScriptStream stream(text);
		expect(stream, "animgroup");
		expect(stream, "{");

		std::vector<AnimationFile> files;
		while (true)
		{
			if (!stream.hasNext())
			{
				throw mt_ex("The animation group is missing '}'");
			}

			const std::string token = stream.token();
			if (token == "}")
			{
				break;
			}
			if (tools::lowercase(token) != "animfile")
			{
				throw mt_ex("Unknown animation group token '" + token + "' (line " + std::to_string(stream.line() + 1) + ")");
			}

			AnimationFile file;
			file.mdx = stream.token();
			if (file.mdx.empty() || file.mdx == "{")
			{
				throw mt_ex("Expected an MDX file name in the animation group");
			}
			expect(stream, "{");

			// Sequence lines, or #include of an .aninc file, until the closing brace
			while (true)
			{
				if (!stream.hasNext())
				{
					throw mt_ex("The animation file " + file.mdx + " is missing '}'");
				}

				const std::string next = stream.peekNext();
				if (next == "}")
				{
					stream.token();
					break;
				}

				if (next == "#include")
				{
					stream.token();
					const std::string path = stream.token(false);
					ScriptStream included(include(path));
					auto animations = parseAnincLines(included);
					if (included.hasNext())
					{
						throw mt_ex("Unexpected '" + included.token() + "' in " + path);
					}
					file.animations.insert(file.animations.end(), animations.begin(), animations.end());
					continue;
				}

				Animation animation;
				animation.name = stream.token();
				animation.startFrame = parseNumber(stream, "first frame");
				animation.numFrames = parseNumber(stream, "length");
				animation.loop = parseNumber(stream, "looping") != 0;
				animation.fps = parseNumber(stream, "fps");
				animation.moveSpeed = parseNumber(stream, "move speed");
				animation.transition = parseNumber(stream, "transition");
				animation.reversed = parseNumber(stream, "reversed") != 0;
				if (animation.startFrame < 0 || animation.numFrames < 0)
				{
					throw mt_ex("Invalid frame range for animation: " + animation.name);
				}
				file.animations.push_back(animation);
			}

			files.push_back(std::move(file));
		}

		return files;
	}

	void loadAnimationGroup(skeletal::SkeletalModel &model, const std::vector<AnimationFile> &files,
							const std::function<Ref<mt::io::MTFile>(const std::string &)> &load)
	{
		using skeletal::SkeletalModel;

		auto loadFile = [&](const AnimationFile &file, SkeletalModel &target) {
			auto data = load(file.mdx);
			if (!data || !data->valid())
			{
				throw mt_ex("Animation file not found: " + file.mdx);
			}
			try
			{
				loadMDXAnimation(&target, data);
			}
			catch (const std::exception &ex)
			{
				throw mt_ex(file.mdx + ": " + ex.what());
			}
		};

		// Restored if a file fails, the model is only changed when every file loads
		const std::vector<skeletal::Joint> joints = model.joints;
		const std::vector<Bounds> bounds = model.bounds;
		const std::vector<Animation> animations = model.animations;
		const int torsoParent = model.torsoParent;
		const int currentFrame = model.currentFrame;

		try
		{
			std::vector<Animation> sequences;
			for (size_t i = 0; i < files.size(); i++)
			{
				const auto &file = files[i];
				const int offset = i == 0 ? 0 : model.numFrames();
				int count = 0;

				if (i == 0)
				{
					// The first file goes straight into the model: it makes the bones of an MDM and checks the weights
					loadFile(file, model);
					count = model.numFrames();
				}
				else
				{
					SkeletalModel scratch;
					scratch.joints = model.joints;
					for (auto &joint : scratch.joints)
					{
						joint.frames.clear();
					}
					scratch.hasBindPose = model.hasBindPose;
					loadFile(file, scratch);
					count = scratch.numFrames();

					for (size_t j = 0; j < model.joints.size(); j++)
					{
						auto &source = scratch.joints[j].frames;
						model.joints[j].frames.insert(model.joints[j].frames.end(), source.begin(), source.end());
					}
					model.bounds.insert(model.bounds.end(), scratch.bounds.begin(), scratch.bounds.end());
					model.torsoParent = scratch.torsoParent;
				}

				for (const auto &animation : file.animations)
				{
					if (animation.numFrames > 0
						&& (animation.startFrame >= count || animation.lastFrame() >= count || animation.lastFrame() < 0))
					{
						throw mt_ex("Animation " + animation.name + " is outside the frames of " + file.mdx);
					}
					Animation moved = animation;
					moved.startFrame += offset;
					sequences.push_back(moved);
				}
			}
			model.animations = std::move(sequences);
		}
		catch (...)
		{
			model.joints = joints;
			model.bounds = bounds;
			model.animations = animations;
			model.torsoParent = torsoParent;
			model.currentFrame = currentFrame;
			throw;
		}
	}

	std::vector<Animation> parseHeadAnimations(const std::string &text)
	{
		static const std::array<const char *, 16> names = {
			"HD_IDLE1",			"HD_IDLE2",			"HD_IDLE3",			"HD_IDLE4", "HD_IDLE5", "HD_IDLE6",	 "HD_IDLE7",	  "HD_IDLE8",
			"HD_DAMAGED_IDLE1", "HD_DAMAGED_IDLE2", "HD_DAMAGED_IDLE3", "HD_LEFT",	"HD_RIGHT", "HD_ATTACK", "HD_ATTACK_END", "HD_PAIN",
		};

		ScriptStream stream(text);
		std::vector<Animation> animations;
		for (const char *name : names)
		{
			if (!stream.hasNext())
			{
				throw mt_ex("The head animation file has " + std::to_string(animations.size()) + " animations, "
							+ std::to_string(names.size()) + " are needed");
			}

			Animation animation;
			animation.name = name;
			animation.startFrame = parseNumber(stream, "first frame");
			animation.numFrames = parseNumber(stream, "length");
			// The game reads the fps as a float, 0 means 1
			const std::string fpsToken = stream.token();
			float fps = 0;
			try
			{
				fps = std::stof(fpsToken);
			}
			catch (const std::exception &)
			{
				throw mt_ex("Invalid fps: '" + fpsToken + "'");
			}
			animation.fps = std::max(1, int(std::lround(fps)));
			const int loopFrames = parseNumber(stream, "looping frames");
			animation.loop = loopFrames > 0;
			if (animation.startFrame < 0 || animation.numFrames < 0)
			{
				throw mt_ex("Invalid frame range for head animation " + animation.name);
			}
			animations.push_back(animation);
		}
		return animations;
	}
}
