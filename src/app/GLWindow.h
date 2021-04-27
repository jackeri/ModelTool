#pragma once

#include "mt.h"
#include "renderer.h"

namespace mt {
	class GLWindow {
	public:
		GLWindow(std::string title);
		~GLWindow();

		void destroy();

		bool init();

		GLFWwindow* system();

		void loop();

	private:
		std::string title;
		GLFWwindow *window;
		Ref<Renderer> renderer = nullptr;
	};
}