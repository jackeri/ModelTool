#pragma once

#include "mt.h"
#include "Camera.h"
#include "renderer.h"
#include "Framebuffer.h"

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
		Ref<Framebuffer> fbo;
		Ref<Renderer> renderer;
		bool captureMouse = false;
		bool sceneHovered = false;
	};
}
