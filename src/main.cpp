#include <string>
#include "app/application.h"

int main(int argc, char *argv[])
{
	auto app = std::make_unique<mt::Application>("Model Tool");

	// Init windowing and other requirements
	if (!app->init())
	{
		return EXIT_FAILURE;
	}

	// Loop until app is ready to close
	app->loop();

	return EXIT_SUCCESS;
}

#ifdef _WIN32

int WINAPI WinMain(
		_In_ HINSTANCE hInstance,
		_In_opt_ HINSTANCE hPrevInstance,
		_In_ LPSTR lpCmdLine,
		_In_ int nShowCmd)
{
	return main(__argc, __argv);
}

#endif