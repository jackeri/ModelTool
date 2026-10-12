#pragma once

#include "mt.h"
#include "model.h"

namespace mt {

	/*! Command line options */
	struct AppOptions {
		std::vector<std::string> basePaths{};  ///< --base: folders (or pk3 files) to load files from
		std::string model{};				   ///< --model: model to open
		std::vector<std::string> animations{}; ///< --animation: animations to load into the model
		std::string hitboxes{};				   ///< --hitboxes: hitbox file to load into the model
		int frame = FRAME_BIND_POSE;		   ///< --frame: frame to show (-1 is the bind pose)
		std::string screenshot{};			   ///< --screenshot: render the scene to this PNG and exit
		bool screenshotUi = false;			   ///< --ui: the screenshot has the whole window with the UI
		int width = 1280;					   ///< --size WxH: screenshot size
		int height = 720;
		std::vector<std::string> view{};	///< --view: comma separated view toggles, like wireframe,skeleton,tags
		std::vector<std::string> exports{}; ///< --export: write the loaded model to these files (format by extension)
		int frames = 0;						///< --frames: close the window after this many frames (for automated runs), 0 runs until closed

		/**
		 * Parse the command line
		 * @param argc argument count
		 * @param argv arguments
		 * @return parsed options
		 * @throws mt::mt_exception on invalid arguments
		 */
		static AppOptions parse(int argc, char *argv[]);

		/**
		 * @return usage text
		 */
		static std::string usage();
	};
}
