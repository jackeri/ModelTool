#include "library.h"
#include <random>

std::string mt::tools::fixName(std::string name, bool mustEndWithSeparator)
{
	while (!name.empty() && name[0] == '/')
	{
		name = name.substr(1);
	}

	if (!name.empty() && mustEndWithSeparator)
	{
		if (name[name.length() - 1] != '/')
		{
			name += '/';
		}
	}

	return name;
}

std::string mt::tools::randomString(size_t length)
{
	std::minstd_rand random;
	random.seed(std::random_device()());

	auto randomChar = [&random]() -> char {
		const char charset[] = "0123456789"
							   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
							   "abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[random() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randomChar);
	return str;
}

bool mt::tools::endsWith(const std::string &str, const std::string &suffix)
{
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool mt::tools::startsWith(const std::string &str, const std::string &prefix)
{
	return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}
