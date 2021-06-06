#include "gtest/gtest.h"
#include "mt.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main(int argc, char **argv)
{
	// Register the logger the test classes might use but just disable the output
	auto logger = spdlog::stdout_color_mt(mt::MT_LOGGER);
	logger->set_level(spdlog::level::off);
	spdlog::set_default_logger(logger);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}