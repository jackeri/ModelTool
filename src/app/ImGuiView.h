#pragma once

#include "mt.h"
#include "FileBrowser.h"
#include <unordered_map>

namespace mt {

	class GLWindow;

	class ImGuiView {

	public:
		ImGuiView() = default;

		~ImGuiView() = default;

		bool setup(GLWindow *window, const char *glslVersion);

		void startFrame();

		void endFrame();

		void draw();

		void shutdown();

	private:

		void drawMenu();

		std::vector<std::function<void()>> m_lateExecution{};

		FileBrowser browser{};
	};
}
