#include "app/mt.h"
#include "app/application.h"
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char *argv[])
{
	// Register the application logger
	auto logger = spdlog::stdout_color_mt(mt::MT_LOGGER);
#ifdef NDEBUG
	logger->set_level(spdlog::level::info);
#else
	logger->set_level(spdlog::level::debug);
#endif
	spdlog::set_default_logger(logger);

	logger->info("Starting application");

	auto app = std::make_unique<mt::Application>("Model Tool");

	// Init windowing and other requirements
	if (!app->init())
	{
		logger->error("Application init failed");
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