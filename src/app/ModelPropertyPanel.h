#pragma once

#include "mt.h"

namespace mt {
	class ModelPropertyPanel {
	public:
		ModelPropertyPanel() = default;

		~ModelPropertyPanel() = default;

		void render();

	private:

		void drawHitboxMenu();
	};
}
