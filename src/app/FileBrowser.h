#pragma once

#include "mt.h"
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
		std::function<void(std::string)> browserCallback{}; ///< Callback function to be used
	};
}
