#pragma once

#include "mt.h"
#include <imfilebrowser.h>
#include <unordered_map>

namespace mt {

	class GLWindow;

	class ImGuiView {

	public:
		ImGuiView() = default;

		bool setup(GLWindow *window, const char *glslVersion);

		void startFrame();

		void endFrame();

		void draw();

		void shutdown();

	private:

		void drawMenu();

		std::unordered_map<std::string, const std::function<void(bool &)>> m_uiLayers{};

		std::vector<std::function<void()>> m_lateExecution{};

		ImGui::FileBrowser fileBrowser{ImGuiFileBrowserFlags_SelectDirectory};
	};
}
