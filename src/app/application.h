#pragma once

#include "gl_window.h"

namespace mt {

	/*! Main application class */
	class Application {
	  public:
		explicit Application(const std::string &title, AppOptions options = {});

		~Application();

		/**
		 * Initialize the application subsystem
		 * @return true if the initialization was successful
		 */
		bool init();

		/**
		 * Application main loop, will block as long as the application is running
		 */
		void loop();

		/**
		 * Run the application: the main loop, or a single screenshot with --screenshot
		 * @return process exit code
		 */
		int run();

	  private:
		Ref<GLWindow> window = nullptr; ///< Main window instance
		AppOptions options;				///< command line options
	};
}
