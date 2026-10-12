#pragma once

#include "library.h"

#include <map>

namespace mt {

	/*! Application settings that are kept between runs */
	struct AppSettings {
		std::vector<std::string> basePaths{};	///< base folders (or pk3 files) the models are loaded from
		std::vector<std::string> recentFiles{}; ///< most recent first
		glm::ivec2 windowPosition{-1, -1};		///< -1 lets the system place the window
		glm::ivec2 windowSize{1280, 720};
		bool windowMaximized = false;
		bool vsync = true;
		bool showLog = true;
		std::map<std::string, std::string> values{}; ///< other settings (view options and such) by name

		static constexpr size_t MAX_RECENT_FILES = 10;

		/**
		 * Put a file first in the recent files (an existing entry moves to the top)
		 * @param path file path
		 */
		void addRecentFile(const std::string &path);

		/**
		 * @return the settings as text
		 */
		[[nodiscard]] std::string serialize() const;

		/**
		 * Read settings from text, unknown keys are kept in values
		 * @param text settings text
		 * @return the settings
		 * @throws mt::mt_exception on invalid text
		 */
		static AppSettings parse(const std::string &text);

		/**
		 * Load the settings from a file
		 * @param path settings file
		 * @return the settings, defaults if the file does not exist or can not be read
		 */
		static AppSettings load(const std::string &path);

		/**
		 * Save the settings to a file, creating the folder when needed
		 * @param path settings file
		 * @return true if the file was written
		 */
		[[nodiscard]] bool save(const std::string &path) const;

		// Typed access to values
		[[nodiscard]] bool getBool(const std::string &key, bool fallback) const;
		[[nodiscard]] int getInt(const std::string &key, int fallback) const;
		[[nodiscard]] float getFloat(const std::string &key, float fallback) const;
		void set(const std::string &key, bool value);
		void set(const std::string &key, int value);
		void set(const std::string &key, float value);
	};

	/**
	 * Folder for the settings: ~/Library/Application Support/ModelTool on macOS, %APPDATA%\ModelTool on Windows and
	 * $XDG_CONFIG_HOME/ModelTool (or ~/.config/ModelTool) elsewhere
	 * @return settings folder, empty if it can not be determined
	 */
	std::string settingsFolder();
}
