#pragma once

#include "mt.h"
#include "Camera.h"

namespace mt {
	class ScenePanel {
	public:
		ScenePanel();

		~ScenePanel();

		void render();

		void onMouseMove(double x, double y, int button);

	private:
		glm::vec2 size{};
		Camera camera;
		bool captureMouse = false;
		bool sceneHovered = false;
	};
}
