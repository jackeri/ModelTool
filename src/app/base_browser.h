#pragma once

#include "mt.h"

namespace mt {

	/*! Browser for the files in the base folders and their pk3 files (the game's virtual file system) */
	class BaseBrowser {
	  public:
		/**
		 * Open the browser
		 * @param title window title
		 * @param extensions file extensions to show (with the dot)
		 * @param callback called with the selected path (relative to the base folders)
		 */
		void show(const std::string &title, std::vector<std::string> extensions, std::function<void(const std::string &)> callback);

		/**
		 * Draw the browser when it is open
		 */
		void render();

	  private:
		bool open = false;
		std::string title{};
		std::string folder{}; ///< current folder, empty for the root
		std::string filter{};
		std::vector<std::string> extensions{};
		std::function<void(const std::string &)> callback{};
	};
}
