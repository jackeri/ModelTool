#include "mt.h"
#include "application.h"
#include "mt_version.h"
#include "log_panel.h"
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char *argv[])
{
	// Register the application logger, it writes to the console and to the log panel
	auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto logger = std::make_shared<spdlog::logger>(mt::MT_LOGGER, spdlog::sinks_init_list{console, mt::LogBuffer::instance()});
	spdlog::register_logger(logger);
#ifdef NDEBUG
	logger->set_level(spdlog::level::info);
#else
	logger->set_level(spdlog::level::debug);
#endif
	spdlog::set_default_logger(logger);

	logger->info("Starting application");

	mt::AppOptions options;
	try
	{
		options = mt::AppOptions::parse(argc, argv);
	}
	catch (const std::exception &ex)
	{
		logger->error("{}", ex.what());
		return EXIT_FAILURE;
	}

	logger->info("ModelTool {}", MT_VERSION);
	auto app = std::make_unique<mt::Application>(std::string("Model Tool ") + MT_VERSION, options);

	// Init windowing and other requirements
	if (!app->init())
	{
		logger->error("Application init failed");
		return EXIT_FAILURE;
	}

	// Loop until app is ready to close (or render the screenshot)
	return app->run();
}

#ifdef _WIN32

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	return main(__argc, __argv);
}

#endif
