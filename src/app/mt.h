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

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

namespace mt {

	const int MAX_PATH = 1024;

	template<class X>
	X &singleton()
	{
		static X x;
		return x;
	}

	template <class T>
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

	using byte_buffer = Ref<std::vector<byte>>;

	inline byte_buffer buffer(size_t size)
	{
		return make_ref<std::vector<byte>>(size);
	}

}
