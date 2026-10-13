#pragma once

#include "library.h"
#include "files.h"
#include "model.h"
#include "skeletal_model.h"
#include "skin.h"

#include <functional>

namespace mt::model {

	/*! ET character file (.char), as read by the game (bg_character.c) */
	struct CharacterDef {
		std::string skin{};					///< skin name: the body skin is <mesh without extension>_<skin>.skin
		std::string mesh{};					///< body model (.mdm)
		std::string animationGroup{};		///< .anim file: the MDX files and their sequences
		std::string animationScript{};		///< .script file (movement states, not used here)
		std::string undressedCorpseModel{}; ///< body of a looted corpse
		std::string undressedCorpseSkin{};	///< skin name of the corpse: <corpse model without extension>_<name>.skin
		std::string hudHead{};				///< head model (.md3), drawn on the body's tag_head in the game too
		std::string hudHeadAnims{};			///< head animation file
		std::string hudHeadSkin{};			///< skin file of the head

		/**
		 * @return path of the body skin file
		 */
		[[nodiscard]] std::string skinPath() const;

		/**
		 * @return path of the undressed corpse skin file, empty without a corpse model
		 */
		[[nodiscard]] std::string corpseSkinPath() const;
	};

	/**
	 * Parse a character file: characterDef { key "value" ... }. Keys ignore the case like in the game.
	 * @param text file text
	 * @return parsed character
	 * @throws mt::mt_exception on an unknown key, a missing value or a missing mesh
	 */
	CharacterDef parseCharacter(const std::string &text);

	/*! Accessory model of an ET body skin (md3_hat, md3_back, ...) */
	struct Accessory {
		std::string type{};	 ///< skin line name, like md3_hat
		std::string model{}; ///< model file
		bool onHead = false; ///< attached to the head model, else to the body
		std::string tag{};	 ///< tag of the body or the head the accessory follows

		/**
		 * The game loads an optional skin for the accessory: <model without extension>_<skin>.skin
		 * @param skin character skin name
		 * @return path of the accessory skin
		 */
		[[nodiscard]] std::string skinPath(const std::string &skin) const;
	};

	/**
	 * The accessories of a body skin in the game's order (body ones, then head ones). Like the game, md3_beltr goes on
	 * tag_bright, md3_beltl on tag_bleft, md3_belt on tag_ubelt, md3_back on tag_back, md3_weapon(2) on
	 * tag_weapon(2), and the head accessories (md3_hat, md3_rank, md3_hat2, md3_hat3) on the head's tag_mouth.
	 * Other md3_ lines are not used by the game and are skipped.
	 * @param skin body skin
	 * @return accessories
	 */
	std::vector<Accessory> characterAccessories(const Skin &skin);

	/*! animfile entry of an animation group: an MDX file and the sequences in it */
	struct AnimationFile {
		std::string mdx{};
		std::vector<Animation> animations{}; ///< frames are relative to the MDX file
	};

	/**
	 * Parse an animation group (.anim): animgroup { animfile "x.mdx" { sequences or #include "x.aninc" } ... }
	 * @param text file text
	 * @param include returns the text of an included file (throws if it is missing)
	 * @return the animation files in order
	 * @throws mt::mt_exception on invalid data
	 */
	std::vector<AnimationFile> parseAnimationGroup(const std::string &text, const std::function<std::string(const std::string &)> &include);

	/**
	 * Load the frames of every MDX file of an animation group into a skeletal model, one after another, and the
	 * sequences with their frames moved to match. The existing frames and sequences are replaced.
	 * @param model model with the bones of the MDX files
	 * @param files animation group files
	 * @param load returns an MDX file
	 * @throws mt::mt_exception if a file is missing or does not match the bones, the model is left unchanged
	 */
	void loadAnimationGroup(skeletal::SkeletalModel &model, const std::vector<AnimationFile> &files,
							const std::function<Ref<mt::io::MTFile>(const std::string &)> &load);

	/**
	 * Parse a head animation file: "first length fps looping" lines, named HD_IDLE1 ... HD_PAIN like in the game
	 * (16 animations, the lines after them are not used)
	 * @param text file text
	 * @return the head sequences
	 * @throws mt::mt_exception on invalid data or too few lines
	 */
	std::vector<Animation> parseHeadAnimations(const std::string &text);
}
