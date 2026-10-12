#pragma once

#include "mt.h"
#include "icons_font_awesome.h"

// FontAwesome icons instead of the browser's "[D]" / "[F]" text (the fork of imgui-filebrowser reads these)
#define IMGUI_FILEBROWSER_DIR_PREFIX ICON_FA_FOLDER " "
#define IMGUI_FILEBROWSER_FILE_PREFIX ICON_FA_FILE_O " "
#define IMGUI_FILEBROWSER_REFRESH_LABEL ICON_FA_REFRESH
#define IMGUI_FILEBROWSER_NEW_DIR_LABEL ICON_FA_PLUS
#define IMGUI_FILEBROWSER_EDIT_PATH_LABEL ICON_FA_PENCIL
#include <imfilebrowser.h>

namespace mt {

	/*! Filesystem browser for folder or file picking */
	class FileBrowser {
	  public:
		FileBrowser() = default;

		~FileBrowser() = default;

		/**
		 * Setup and show the FileBrowser
		 * @param title title of the window
		 * @param callback callback to be used when a file/folder is selected
		 * @param folderSelector is the picker in File or Folder picking mode
		 */
		void show(const std::string &title, std::function<void(std::string)> callback, bool folderSelector = false);

		/**
		 * Set the file extension filter for the picker
		 */
		void setTypeFilters(const std::vector<std::string> &);

		/**
		 * Set the folder the picker shows
		 * @param folder folder path, ignored if it does not exist
		 * @return true if the folder was set
		 */
		bool setDirectory(const std::string &folder);

		/**
		 * Returns the current picker status
		 * @return true if open
		 */
		bool isOpen();

		/**
		 * ImGui main loop that draws the controls and handles the inputs
		 */
		void handle();

	  private:
		std::unique_ptr<ImGui::FileBrowser> browser = nullptr; ///< FileBrowser instance, can either be a file picker or a folder picker
		std::function<void(std::string)> browserCallback{};	   ///< Callback function to be used
	};
}
