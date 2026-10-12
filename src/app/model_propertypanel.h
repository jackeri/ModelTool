#pragma once

#include "mt.h"

namespace mt {
	class ModelPropertyPanel {
	  public:
		ModelPropertyPanel() = default;

		~ModelPropertyPanel() = default;

		/**
		 * Draw the model property panel
		 */
		void render();

	  private:
		/**
		 * Draw the hitbox menu
		 */
		void drawHitboxMenu();

		/**
		 * Draw the animation playback controls
		 */
		void drawAnimationMenu();

		/**
		 * Draw the level of detail controls
		 */
		void drawLodMenu();

		std::string sequenceFilter{}; ///< filter for the animation sequence list
	};
}
