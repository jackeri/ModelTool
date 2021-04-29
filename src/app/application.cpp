#include "application.h"
#include "state.h"

namespace mt {

	Application::Application(const std::string &title)
	{
		// Ini the application state structure
		singleton<State>();

		window = make_ref<GLWindow>(title);
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
}