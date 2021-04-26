#pragma once

#include <vector>

#include "GL/glew.h"

namespace mt {

	template<class X>
	X &singleton()
	{
		static X x;
		return x;
	}

	const int MAX_PATH = 1024;

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
