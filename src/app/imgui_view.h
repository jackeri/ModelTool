#pragma once

#include "mt.h"
#include "file_browser.h"
#include "base_browser.h"
#include "log_panel.h"
#include <unordered_map>

namespace mt {

	class GLWindow;
	class ScenePanel;

	class ImGuiView {
	  public:
		ImGuiView();

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

		/**
		 * Scene panel whose camera the View menu controls
		 * @param panel scene panel
		 */
		void setScene(ScenePanel *panel)
		{
			scene = panel;
		}

	  private:
		/**
		 * Draw the menu bar
		 */
		void drawMenu();

		/**
		 * Draw the Edit menu (model operations)
		 */
		void drawEditMenu();

		/**
		 * Draw the bounding box scaling window when it is open
		 */
		void drawScaleBoundsWindow();

		/**
		 * Draw the File > Export submenu
		 */
		void drawExportMenu();

		/**
		 * Draw the "Export all" window when it is open
		 */
		void drawExportAllWindow();

		/**
		 * Handle the keyboard shortcuts (not while typing into a text field)
		 */
		void handleShortcuts();

		/**
		 * Load a model and log the result
		 * @param path model path (relative to the base folders or a path on the disk)
		 */
		void openModel(const std::string &path);

		/**
		 * Show the file browser for opening a model from the disk
		 */
		void openModelFromDisk();

		/**
		 * Folder the open file browsers start in: the folder of the last opened model or animation, or the first base
		 * folder when nothing has been opened yet
		 */
		[[nodiscard]] std::string startFolder() const;

		/**
		 * Remember the folder of an opened file for the next file browser (kept in the settings)
		 * @param file opened file
		 */
		void rememberFolder(const std::string &file);

		/**
		 * Show the folder browser for the base folder
		 * @param add add the folder to the current ones instead of replacing them
		 */
		void showBaseFolderBrowser(bool add);

		/**
		 * Shortcut text for menus, Ctrl is shown as Cmd on macOS
		 */
		static std::string shortcutLabel(const std::string &label);

		std::vector<std::function<void()>>
			m_lateExecution{}; ///< Late execution buffer for code that must not be executed in the menu context

		FileBrowser browser{}; ///< File browser instance which is changed by requirements

		std::shared_ptr<spdlog::logger> logger; ///< shared logger

		ScenePanel *scene = nullptr;  ///< scene panel for the camera settings
		bool showScaleBounds = false; ///< the bounding box scaling window is open
		BaseBrowser baseBrowser{};	  ///< browser for the base folders and pk3 files
		LogPanel logPanel{};		  ///< log window
		GLFWwindow *glfwWindow = nullptr;
		std::string iniPath{};				///< ImGui layout file, kept alive for ImGuiIO::IniFilename
		bool askedBaseFolder = false;		///< the base folder was asked for once
		bool showExportAll = false;			///< the export all window is open
		std::string exportFolder{};			///< export all: output folder
		std::string exportName{};			///< export all: file name without the extension
		std::vector<bool> exportSelected{}; ///< export all: selected formats
	};
}
