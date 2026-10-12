#include "application.h"
#include "state.h"

namespace mt {

	Application::Application(const std::string &title, AppOptions appOptions) : options(std::move(appOptions))
	{
		// Init the application state structure
		singleton<State>();

		window = make_ref<GLWindow>(title, options);
	}

	Application::~Application()
	{
		if (this->window)
		{
			window->destroy();
		}

		singleton<State>().clear();
	}

	bool Application::init()
	{
		return window->init();
	}

	void Application::loop()
	{
		window->loop();
	}

	int Application::run()
	{
		if (!options.screenshot.empty())
		{
			return window->screenshot() ? EXIT_SUCCESS : EXIT_FAILURE;
		}

		loop();
		return EXIT_SUCCESS;
	}
}