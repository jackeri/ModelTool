#pragma once

#include "mt.h"
#include "renderer.h"

#include "model_propertypanel.h"
#include "scene_panel.h"
#include "imgui_view.h"

namespace mt {

	/*! Main window with the OpenGL context */
	class GLWindow {
	public:
		explicit GLWindow(std::string title);

		~GLWindow();

		/**
		 * Destroy the opengl context and close the window
		 */
		void destroy();

		/**
		 * Initialize the window and the OpenGL subsystem
		 * @return was init successful
		 */
		bool init();

		/**
		 * GLFW window instance
		 * @return glfwwindow
		 */
		GLFWwindow *system();

		/**
		 * Window main loop for input and drawing
		 */
		void loop();

		/**
		 * Callback for key presses
		 * @param key key code
		 * @param scancode code
		 * @param action what actions
		 * @param mods key modifiers active
		 */
		void onKey(int key, int scancode, int action, int mods);

		/**
		 * Mouse scroll callback
		 * @param delta delta difference since last call
		 */
		void onScroll(double delta);

		/**
		 * Window resize callback
		 * @param width width of the window
		 * @param height height of the window
		 */
		void onResize(int width, int height);

	private:
		/**
		 * Handle window input
		 */
		void handleInput();

		std::string title; ///< Window title
		GLFWwindow *window{}; ///< GLFW window instance
		Ref<ImGuiView> view; ///< UI instance
		Ref<ModelPropertyPanel> propertyPanel = nullptr; ///< Model property panels instance to be drawn
		Ref<ScenePanel> scenePanel = nullptr; ///< 3d scene panel instance to be drawn
	};
}