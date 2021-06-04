#pragma once

#include "mt.h"
#include "glwindow.h"

namespace mt {

	/*! Main application class */
	class Application {
	public:
		explicit Application(const std::string &title);

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

	private:
		Ref<GLWindow> window = nullptr; ///< Main window instance
	};
}
