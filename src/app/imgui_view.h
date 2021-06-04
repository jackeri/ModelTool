#pragma once

#include "mt.h"
#include "file_browser.h"
#include <unordered_map>

namespace mt {

	class GLWindow;

	class ImGuiView {

	public:
		ImGuiView() = default;

		~ImGuiView() = default;

		/**
		 * Setup ImGui for use
		 * @param window glfw window instance
		 * @param glslVersion used glsl version
		 * @return was the setup successful
		 */
		bool setup(GLWindow *window, const char *glslVersion);

		/**
		 * Prime ImGui for a new frame
		 */
		void startFrame();

		/**
		 * Tell ImGui that all drawing call have been made on this frame
		 */
		void endFrame();

		/**
		 * Tell ImGui to actually draw the requested elements
		 */
		void draw();

		/**
		 * Shutdown the ImGui subsystem and unbind everything
		 */
		void shutdown();

	private:

		/**
		 * Draw the menu bar
		 */
		void drawMenu();

		std::vector<std::function<void()>> m_lateExecution{}; ///< Late execution buffer for code that must not be executed in the menu context

		FileBrowser browser{}; ///< File browser instance which is changed by requirements
	};
}
