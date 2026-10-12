#pragma once

#include "mt.h"
#include "filesystem.h"
#include "fuzzy.h"

namespace mt {

	/**
	 * File picker for the game's virtual file system: the base folders are the root, and the contents of their pk3
	 * files and pk3dir folders show up as if they were normal files and folders
	 */
	class BaseBrowser {
	  public:
		/**
		 * Open the picker
		 * @param title window title
		 * @param extensions file extensions to show (with the dot), empty for every file
		 * @param callback called with the selected path (relative to the root)
		 */
		void show(const std::string &title, std::vector<std::string> extensions, std::function<void(const std::string &)> callback);

		/**
		 * Draw the picker when it is open
		 */
		void render();

	  private:
		/**
		 * Go to a folder of the virtual file system
		 * @param path folder path, empty for the root
		 */
		void enterFolder(const std::string &path);

		/**
		 * Read the current folder again
		 */
		void refresh();

		/**
		 * Draw the path bar (root and every folder of the current path as buttons)
		 */
		void drawPathBar();

		/**
		 * Draw the file and folder table
		 * @return the file to open when one was double clicked
		 */
		std::string drawTable();

		/**
		 * Draw the fuzzy search results over every file of the virtual file system
		 * @return the file to open when one was double clicked
		 */
		std::string drawSearchResults();

		/**
		 * Run the search again when the text or the type filter changed
		 */
		void updateSearch();

		/**
		 * Is a file shown with the current type filter
		 */
		[[nodiscard]] bool typeShown(const std::string &name) const;

		/**
		 * @param name file or folder name in the current folder
		 * @return the path relative to the root
		 */
		[[nodiscard]] std::string pathOf(const std::string &name) const;

		/**
		 * Open the selected file, or enter the folder typed in the path field
		 * @param path path relative to the root
		 * @return true if a file was opened
		 */
		bool accept(const std::string &path);

		bool open = false;			  ///< the picker is shown
		bool openPopup = false;		  ///< the modal has to be opened on the next frame
		std::string title{};		  ///< window title
		std::string folder{};		  ///< current folder, empty for the root
		std::string search{};		  ///< fuzzy search over every file, empty to browse the folders
		bool focusSearch = false;	  ///< put the keyboard focus on the search field
		bool searchSubmitted = false; ///< Enter was pressed in the search field
		std::string selected{};		  ///< selected entry name in the current folder
		std::string pathInput{};	  ///< the path field (selected file relative to the root)
		bool allTypes = false;		  ///< show every file instead of the given extensions
		std::vector<std::string> extensions{};
		std::vector<io::VirtualEntry> entries{}; ///< listing of the current folder
		std::vector<io::VirtualFile> allFiles{}; ///< every file, read on the first search
		bool allFilesRead = false;				 ///< allFiles is up to date
		std::vector<size_t> candidates{};		 ///< allFiles shown with the type filter
		std::vector<FuzzyResult> results{};		 ///< search results (indexes into candidates), best first
		size_t matchCount = 0;					 ///< all matches (only the best ones are shown)
		std::string searchedText{};				 ///< search text of the results
		bool searchedAllTypes = false;			 ///< type filter of the results
		int selectedResult = 0;					 ///< selected search result
		bool scrollToResult = false;			 ///< keep the selected result visible
		std::function<void(const std::string &)> callback{};
	};
}
