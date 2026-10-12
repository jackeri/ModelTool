#include "player.h"

#include <filesystem>

namespace mt::model {

	namespace {
		/*! Animation names of the game (bg_public.h animNumber_t), in the order of the file */
		const std::vector<std::pair<const char *, PlayerPart>> &animationNames()
		{
			static const std::vector<std::pair<const char *, PlayerPart>> names = {
				{"BOTH_DEATH1", PlayerPart::Both},	   {"BOTH_DEAD1", PlayerPart::Both},	  {"BOTH_DEATH2", PlayerPart::Both},
				{"BOTH_DEAD2", PlayerPart::Both},	   {"BOTH_DEATH3", PlayerPart::Both},	  {"BOTH_DEAD3", PlayerPart::Both},
				{"TORSO_GESTURE", PlayerPart::Torso},  {"TORSO_ATTACK", PlayerPart::Torso},	  {"TORSO_ATTACK2", PlayerPart::Torso},
				{"TORSO_DROP", PlayerPart::Torso},	   {"TORSO_RAISE", PlayerPart::Torso},	  {"TORSO_STAND", PlayerPart::Torso},
				{"TORSO_STAND2", PlayerPart::Torso},   {"LEGS_WALKCR", PlayerPart::Legs},	  {"LEGS_WALK", PlayerPart::Legs},
				{"LEGS_RUN", PlayerPart::Legs},		   {"LEGS_BACK", PlayerPart::Legs},		  {"LEGS_SWIM", PlayerPart::Legs},
				{"LEGS_JUMP", PlayerPart::Legs},	   {"LEGS_LAND", PlayerPart::Legs},		  {"LEGS_JUMPB", PlayerPart::Legs},
				{"LEGS_LANDB", PlayerPart::Legs},	   {"LEGS_IDLE", PlayerPart::Legs},		  {"LEGS_IDLECR", PlayerPart::Legs},
				{"LEGS_TURN", PlayerPart::Legs},	   {"TORSO_GETFLAG", PlayerPart::Torso},  {"TORSO_GUARDBASE", PlayerPart::Torso},
				{"TORSO_PATROL", PlayerPart::Torso},   {"TORSO_FOLLOWME", PlayerPart::Torso}, {"TORSO_AFFIRMATIVE", PlayerPart::Torso},
				{"TORSO_NEGATIVE", PlayerPart::Torso},
			};
			return names;
		}

		constexpr int TORSO_GESTURE = 6;
		constexpr int LEGS_WALKCR = 13;
		constexpr int BASE_ANIMATIONS = 25; ///< animations every player has (up to LEGS_TURN)

		std::string lowercase(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}

		int toInt(const std::string &token)
		{
			try
			{
				return std::stoi(token);
			}
			catch (const std::exception &)
			{
				throw mt_ex("Invalid number in animation.cfg: " + token);
			}
		}

		float toFloat(const std::string &token)
		{
			try
			{
				return std::stof(token);
			}
			catch (const std::exception &)
			{
				throw mt_ex("Invalid number in animation.cfg: " + token);
			}
		}

		std::string folderOf(const std::string &path)
		{
			auto slash = path.find_last_of("/\\");
			return slash == std::string::npos ? std::string() : path.substr(0, slash);
		}

		std::string inFolder(const std::string &folder, const std::string &name)
		{
			return folder.empty() ? name : folder + "/" + name;
		}

		/**
		 * File names in a folder of the source, or of the disk for absolute paths
		 */
		std::vector<std::string> folderFiles(mt::io::FileSource &source, const std::string &folder)
		{
			std::vector<std::string> names;
			std::error_code error;
			if (std::filesystem::path(folder).is_absolute() && std::filesystem::is_directory(folder, error))
			{
				for (const auto &entry : std::filesystem::directory_iterator(folder, error))
				{
					if (entry.is_regular_file(error))
					{
						names.push_back(entry.path().filename().string());
					}
				}
			}
			else
			{
				auto records = source.getFiles(folder);
				for (const auto &record : *records)
				{
					if (!record.isDirectory)
					{
						names.push_back(record.name);
					}
				}
			}
			return names;
		}
	}

	Animation PlayerAnimation::toAnimation() const
	{
		Animation animation;
		animation.name = name;
		animation.numFrames = numFrames;
		animation.fps = fps;
		animation.loop = loopFrames > 0;
		animation.reversed = reversed;
		animation.startFrame = reversed ? firstFrame + numFrames - 1 : firstFrame;
		return animation;
	}

	std::vector<Animation> PlayerConfig::legsAnimations() const
	{
		std::vector<Animation> list;
		for (const auto &animation : animations)
		{
			if (animation.part != PlayerPart::Torso)
			{
				list.push_back(animation.toAnimation());
			}
		}
		return list;
	}

	std::vector<Animation> PlayerConfig::torsoAnimations() const
	{
		std::vector<Animation> list;
		for (const auto &animation : animations)
		{
			if (animation.part != PlayerPart::Legs)
			{
				list.push_back(animation.toAnimation());
			}
		}
		return list;
	}

	PlayerConfig parsePlayerConfig(const std::string &text)
	{
		PlayerConfig config;
		const auto &names = animationNames();
		int skip = 0;

		std::istringstream lines(text);
		for (std::string line; std::getline(lines, line);)
		{
			if (auto comment = line.find("//"); comment != std::string::npos)
			{
				line.erase(comment);
			}
			std::istringstream stream(line);
			std::vector<std::string> tokens;
			for (std::string token; stream >> token;)
			{
				tokens.push_back(token);
			}
			if (tokens.empty())
			{
				continue;
			}

			const std::string key = lowercase(tokens[0]);
			const bool number = std::isdigit((unsigned char)key[0]) || key[0] == '-';
			if (!number)
			{
				if (!config.animations.empty())
				{
					throw mt_ex("Unexpected token in animation.cfg: " + tokens[0]);
				}
				if (key == "sex" && tokens.size() > 1)
				{
					config.sex = lowercase(tokens[1]);
				}
				else if (key == "footsteps" && tokens.size() > 1)
				{
					config.footsteps = lowercase(tokens[1]);
				}
				else if (key == "headoffset" && tokens.size() > 3)
				{
					config.headOffset = {toFloat(tokens[1]), toFloat(tokens[2]), toFloat(tokens[3])};
				}
				else if (key == "fixedlegs")
				{
					config.fixedLegs = true;
				}
				else if (key == "fixedtorso")
				{
					config.fixedTorso = true;
				}
				else
				{
					throw mt_ex("Unknown token in animation.cfg: " + tokens[0]);
				}
				continue;
			}

			if (tokens.size() < 4)
			{
				throw mt_ex("Animation line needs first frame, frames, looping frames and fps: " + line);
			}
			const size_t index = config.animations.size();
			if (index >= names.size())
			{
				break;
			}

			PlayerAnimation animation;
			animation.name = names[index].first;
			animation.part = names[index].second;
			animation.firstFrame = toInt(tokens[0]);
			animation.numFrames = toInt(tokens[1]);
			animation.loopFrames = toInt(tokens[2]);
			animation.fps = toInt(tokens[3]);

			// lower.md3 does not have the torso only frames, the legs frames follow the death frames
			if (animation.part == PlayerPart::Legs)
			{
				if (int(index) == LEGS_WALKCR)
				{
					skip = animation.firstFrame - config.animations[TORSO_GESTURE].firstFrame;
				}
				animation.firstFrame -= skip;
			}
			if (animation.numFrames < 0)
			{
				animation.numFrames = -animation.numFrames;
				animation.reversed = true;
			}
			if (animation.fps <= 0)
			{
				animation.fps = 1;
			}
			config.animations.push_back(animation);
		}

		if (config.animations.size() < BASE_ANIMATIONS)
		{
			throw mt_ex("animation.cfg has " + std::to_string(config.animations.size()) + " animations, " + std::to_string(BASE_ANIMATIONS)
						+ " are needed");
		}
		return config;
	}

	std::string PlayerFiles::skinPath(const std::string &part, const std::string &skin) const
	{
		return inFolder(folder, part + "_" + skin + ".skin");
	}

	bool findPlayerFiles(mt::io::FileSource &source, const std::string &path, PlayerFiles &files)
	{
		std::string name = lowercase(path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1));
		static const std::vector<std::string> partNames = {"animation.cfg", "lower.md3", "upper.md3", "head.md3"};
		if (std::find(partNames.begin(), partNames.end(), name) == partNames.end())
		{
			return false;
		}

		PlayerFiles found;
		found.folder = folderOf(path);
		found.lower = inFolder(found.folder, "lower.md3");
		found.upper = inFolder(found.folder, "upper.md3");
		found.head = inFolder(found.folder, "head.md3");
		found.config = inFolder(found.folder, "animation.cfg");
		for (const auto &part : {found.lower, found.upper, found.head, found.config})
		{
			if (!source.findFile(part))
			{
				return false;
			}
		}

		for (const auto &file : folderFiles(source, found.folder))
		{
			std::string lower = lowercase(file);
			if (tools::startsWith(lower, "lower_") && tools::endsWith(lower, ".skin"))
			{
				found.skins.push_back(file.substr(6, file.size() - 6 - 5));
			}
		}
		std::sort(found.skins.begin(), found.skins.end(),
				  [](const std::string &a, const std::string &b) { return (a == "default") != (b == "default") ? a == "default" : a < b; });

		files = found;
		return true;
	}
}
