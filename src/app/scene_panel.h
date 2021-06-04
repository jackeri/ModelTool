#pragma once

#include "mt.h"
#include "Camera.h"
#include "renderer.h"
#include "Framebuffer.h"

namespace mt {

	/*! Scene panel which hold the actual 3d view */
	class ScenePanel {
	public:
		ScenePanel();

		~ScenePanel();

		/**
		 * Render the scene panel and handle input
		 */
		void render();

		/**
		 * Callback for mouse moved in the scene
		 * @param x X-coordinate of the pointer
		 * @param y Y-coordinate of the pointer
		 * @param button mouse button index which is pressed
		 */
		void onMouseMove(double x, double y, int button);

	private:
		glm::vec2 size{}; ///< Viewport size
		Camera camera; ///< Camera instance which is used in the scene panel
		Ref<Framebuffer> fbo; ///< Frame buffer for 3d view drawing
		Ref<Renderer> renderer; ///< Renderer which is used for the 3d view
		bool captureMouse = false; ///< state flag for mouse capture
		bool sceneHovered = false; ///< state flag for mouse hover scene panel
	};
}
