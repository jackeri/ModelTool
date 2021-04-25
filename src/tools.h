#pragma once

#include "mt.h"
#include <string>

namespace mt::tools {

	template<typename T, typename S>
	inline T *SafeAllocate(S size)
	{ return (T *) (new byte[size_t(size)]); }

	template<typename T>
	inline void SafeFree(T *&data)
	{
		if (data)
		{
			delete[] data;
			data = nullptr;
		}
	}

	std::string randomString(size_t length);

	bool endsWith(const std::string &str, const std::string &suffix);

	bool startsWith(const std::string &str, const std::string &prefix);
}