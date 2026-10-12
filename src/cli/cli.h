#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace mt::cli {

	constexpr int EXIT_OK = 0;	   ///< the command succeeded
	constexpr int EXIT_FAILED = 1; ///< the command failed (a file could not be loaded or written)
	constexpr int EXIT_USAGE = 2;  ///< invalid command line

	/**
	 * Run the command line tool
	 * @param args arguments without the program name
	 * @param out normal output
	 * @param err error messages
	 * @return exit code (EXIT_OK, EXIT_FAILED or EXIT_USAGE)
	 */
	int runCli(const std::vector<std::string> &args, std::ostream &out, std::ostream &err);

	/**
	 * @return usage text
	 */
	std::string usage();
}
