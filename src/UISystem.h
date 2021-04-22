#pragma once

#include <GLFW/glfw3.h>

namespace mt {
	class UISystem {
	public:
		static bool setup(GLFWwindow *window, const char *);
		static void draw();
		static void shutdown();

	private:
		UISystem() = default;
	};
}
