#pragma once

#include "mt.h"
#include "renderer.h"

#include "ModelPropertyPanel.h"
#include "ScenePanel.h"
#include "ImGuiView.h"

namespace mt {
	class GLWindow {
	public:
		explicit GLWindow(std::string title);

		~GLWindow();

		void destroy();

		bool init();

		GLFWwindow *system();

		void loop();

		void onKey(int key, int scancode, int action, int mods);

		void onScroll(double delta);

		void onResize(int width, int height);

	private:
		void handleInput();

		std::string title;
		GLFWwindow *window{};
		Ref<ImGuiView> view;
		Ref<ModelPropertyPanel> propertyPanel = nullptr;
		Ref<ScenePanel> scenePanel = nullptr;
	};
}