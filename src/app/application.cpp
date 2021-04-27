#include "application.h"

namespace mt {

	Application::Application(const std::string &title)
	{
		window = make_ref<GLWindow>(title);
	}

	Application::~Application()
	{
		if (this->window)
		{
			window->destroy();
		}
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