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

		/**
		 * Draw the player model controls (skin, legs and torso sequences)
		 */
		void drawPlayerMenu();

		/**
		 * Draw the attached models (parent, tag, frame, remove)
		 */
		void drawAttachmentsMenu();

		void drawModelBasics();
		void drawTransformMenu();
		void drawMeshesMenu();
		void drawTagsMenu();
		void drawBonesMenu();
		void drawSequencesMenu();

		std::string sequenceFilter{}; ///< filter for the animation sequence list
		std::string boneFilter{};	  ///< filter for the bone list
	};
}
