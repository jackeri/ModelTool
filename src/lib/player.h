#pragma once

#include "library.h"
#include "files.h"
#include "model.h"

namespace mt::model {

	/*! Which part of a player model an animation sequence moves */
	enum class PlayerPart
	{
		Both,  ///< lower and upper (deaths)
		Torso, ///< upper only
		Legs   ///< lower only
	};

	/*! Animation line of an animation.cfg */
	struct PlayerAnimation {
		std::string name{}; ///< game name, like LEGS_WALK
		PlayerPart part = PlayerPart::Both;
		int firstFrame = 0;	   ///< first frame in the part's model (legs are already moved down)
		int numFrames = 0;	   ///< number of frames
		int loopFrames = 0;	   ///< frames at the end that loop, 0 = no loop
		int fps = 0;		   ///< frames per second
		bool reversed = false; ///< played backwards (negative frame count in the file)

		/**
		 * @return the sequence as a model animation (reversed sequences start from their last frame)
		 */
		[[nodiscard]] Animation toAnimation() const;
	};

	/*! Quake 3 style player animation.cfg */
	struct PlayerConfig {
		std::string sex = "m";
		std::string footsteps = "normal";
		glm::vec3 headOffset{0.0f};
		bool fixedLegs = false;
		bool fixedTorso = false;
		std::vector<PlayerAnimation> animations{}; ///< in the game's order

		/**
		 * @return the sequences of lower.md3 (BOTH_* and LEGS_*)
		 */
		[[nodiscard]] std::vector<Animation> legsAnimations() const;

		/**
		 * @return the sequences of upper.md3 (BOTH_* and TORSO_*)
		 */
		[[nodiscard]] std::vector<Animation> torsoAnimations() const;
	};

	/**
	 * Parse an animation.cfg. The animation lines get the game's names in order (BOTH_DEATH1 ... LEGS_TURN, then the
	 * Team Arena TORSO_GETFLAG ... TORSO_NEGATIVE when present). Like the game, the legs frames are moved down by the
	 * torso only frames, because lower.md3 does not have them.
	 * @param text file text
	 * @return parsed config
	 * @throws mt::mt_exception on invalid data or too few animation lines
	 */
	PlayerConfig parsePlayerConfig(const std::string &text);

	/*! Files of a Quake 3 style player model folder */
	struct PlayerFiles {
		std::string folder{};			  ///< folder of the files, without the trailing slash
		std::string lower{};			  ///< lower.md3 (legs, main model)
		std::string upper{};			  ///< upper.md3 (torso, on the lower's tag_torso)
		std::string head{};				  ///< head.md3 (on the upper's tag_head)
		std::string config{};			  ///< animation.cfg
		std::vector<std::string> skins{}; ///< skin names that have a lower_<name>.skin, "default" first

		/**
		 * @param part "lower", "upper" or "head"
		 * @param skin skin name
		 * @return path of the part's skin file
		 */
		[[nodiscard]] std::string skinPath(const std::string &part, const std::string &skin) const;
	};

	/**
	 * Is this file part of a player model folder: animation.cfg, or lower / upper / head .md3 next to an animation.cfg
	 * @param source file source
	 * @param path file path
	 * @param files the player files when found
	 * @return true if the folder has a complete player model
	 */
	bool findPlayerFiles(mt::io::FileSource &source, const std::string &path, PlayerFiles &files);
}
