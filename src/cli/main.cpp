#include <cstdlib>
#include <iostream>
#include "library.h"

int main(int argc, char *argv[])
{
	std::string random = mt::tools::randomString(30);
	if (mt::tools::endsWith("testiketee", "tee"))
	{
		std::cout << "Ends with 'tee'" << std::endl;
	}
	std::cout << "Hello, World! With: " << random << std::endl;
	return EXIT_SUCCESS;
}
