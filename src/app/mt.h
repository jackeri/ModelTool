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
#include <misc/cpp/imgui_stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

namespace mt {

	const int MAX_PATH = 1024;

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

	template<class X>
	Ref<X> singleton_ref()
	{
		static Ref<X> x{new X};
		return x;
	}

	template<class X>
	inline X &singleton()
	{
		// static X x;
		// return x;
		return *singleton_ref<X>().get();
	}

	template<typename T>
	class SingletonType {
	public:
		SingletonType(SingletonType const &) = delete;

		SingletonType &operator=(SingletonType const &) = delete;

		static T &instance()
		{
			return singleton<T>();
		}

		static Ref<T> &instance_ref()
		{
			return singleton_ref<T>();
		}

		friend T &singleton<T>();

		friend Ref<T> singleton_ref<T>();

	protected:
		SingletonType() = default;
	};

	uint64_t constexpr mix(char m, uint64_t s)
	{
		return ((s << 7) + ~(s >> 3)) + ~m;
	}

	uint64_t constexpr hash(const char *m)
	{
		return (*m) ? mix(*m, hash(m + 1)) : 0;
	}
}
