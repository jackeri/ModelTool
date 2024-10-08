#pragma once

#include "mt.h"
#include "model.h"
#include "filesystem.h"

#include <unordered_set>

namespace mt::model {

	/*! Hitbox instance, just a box at the moment */
	struct Hitbox : public Bounds, public Point {
		std::string name{};							 ///< Name of this hitbox
		int parentJoint = -1;						 ///< Parent joint or tag which to link
		std::unordered_set<std::string> modifiers{}; ///< List of modifiers that this hitbox has

		explicit Hitbox(float size = 0);

		/**
		 * Load a list of hitboxes from a file
		 * @param file file to load from
		 * @param list vector to load the hitbox definitions into
		 */
		static void loadHitboxes(const Ref<io::MTFile> &file, std::vector<Hitbox> &list);

		/**
		 * Persist a list of hitbox definitions to a file
		 * @param path local path to save the file to
		 * @param list vector of hitbox definitions
		 */
		static void saveHitboxes(const std::string &path, std::vector<Hitbox> &list);
	};
}
