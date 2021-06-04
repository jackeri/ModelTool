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
	};
}
