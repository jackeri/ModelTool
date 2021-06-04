#pragma once

#include <string>
#include <algorithm>
#include <vector>
#include <set>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <functional>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

namespace mt {

	const int MAX_PATH = 1024; ///< Max path length in the model files (game has a max path length)

	/**
	 * Clears an vector and handles the destruction of the elements
	 * @tparam T type of the vector
	 * @param deleteMe vector to be cleared
	 */
	template<class T>
	inline void emptyVector(std::vector<T> &deleteMe)
	{
		if (deleteMe.empty())
		{
			return;
		}

		std::vector<T>().swap(deleteMe);
	}

	using byte = unsigned char;

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	Ref<T> make_ref(Args... args)
	{
		return std::make_shared<T>(args...);
	}

	template<typename T>
	using RefList = std::shared_ptr<std::vector<T>>;

	template<typename T, typename... Args>
	RefList<T> make_ref_list(Args... args)
	{
		return std::make_shared<std::vector<T>>(args...);
	}

	using byte_buffer = Ref<std::vector<byte>>;

	inline byte_buffer buffer(size_t size)
	{
		return make_ref_list<byte>(size);
	}

	/**
	 * constructs an singleton and keeps it locally in hunk
	 * @tparam X type of the singleton
	 * @return instance of the singleton
	 */
	template<class X>
	Ref<X> singleton_ref()
	{
		static Ref<X> x{new X};
		return x;
	}

	/**
	 * constructs an singleton and keeps it locally in hunk
	 * @tparam X type of the singleton
	 * @return instance of the singleton
	 */
	template<class X>
	inline X &singleton()
	{
		// static X x;
		// return x;
		return *singleton_ref<X>().get();
	}

	/**
	 * Used in conjunction with the hash function
	 * @param character character to hash
	 * @param seed seed number
	 * @return hashed character
	 */
	uint64_t constexpr mix(char character, uint64_t seed)
	{
		return ((seed << 7) + ~(seed >> 3)) + ~character;
	}

	/**
	 * Create a simple hash number from a string, not secure but works great with simple switches
	 * @param string to be hashed
	 * @return hash number
	 */
	uint64_t constexpr hash(const char *string)
	{
		return (*string) ? mix(*string, hash(string + 1)) : 0;
	}
}
