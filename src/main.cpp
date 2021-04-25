#include <string>
#include "app/application.h"

int main()
{
	auto app = std::make_unique<mt::Application>("Model Tool");

	// Init windowing and other requirements
	if (!app->init()) {
		return EXIT_FAILURE;
	}

	// Loop until app is ready to close
	app->loop();

	return EXIT_SUCCESS;
}