#pragma once

#include "mt.h"
#include <string>

namespace mt::tools {

	/**
	 * @deprecated not needed anymore, cleanup at some point
	 * @tparam T return type
	 * @tparam S size type
	 * @param size size of the buffer
	 * @return allocated memory cast to type
	 */
	template<typename T, typename S>
	inline T *SafeAllocate(S size)
	{
		return (T *)(new byte[size_t(size)]);
	}

	/**
	 * @deprecated not needed anymore, cleanup at some point
	 * @tparam T data buffer type
	 * @param data data to be freed
	 */
	template<typename T>
	inline void SafeFree(T *&data)
	{
		if (data)
		{
			delete[] data;
			data = nullptr;
		}
	}

	/**
	 * Generate a random string of a specific length
	 * @param length length of the requested string
	 * @return randomized string
	 */
	std::string randomString(size_t length);

	/**
	 * Checks if a string ends with a suffix
	 * @param str string to check
	 * @param suffix suffix to find
	 * @return true if the string ends with the suffix
	 */
	bool endsWith(const std::string &str, const std::string &suffix);

	/**
	 * Checks if the string starts with a prefix
	 * @param str string to check
	 * @param prefix prefix to find
	 * @return true if the string starts with the prefix
	 */
	bool startsWith(const std::string &str, const std::string &prefix);

	/**
	 * Prints possible OpenGL errors
	 */
	void printGLErrors();
}