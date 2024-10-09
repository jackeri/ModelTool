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

	typedef int reshandle;

	inline int to_int(const float &val)
	{
		return static_cast<int>(std::round(val));
	}

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

	class mt_exception : public std::runtime_error {
		std::string m_msg;

	  public:
		explicit mt_exception(const std::string &msg, const std::string &file = "", const int &line = -1) : std::runtime_error(msg)
		{
			std::ostringstream stream;
			if (!file.empty())
			{
				stream << file;
				if (line >= 0)
				{
					stream << "(" << (line + 1) << "): ";
				}
				else
				{
					stream << ": ";
				}
			}

			if (line >= 0 && !line)
			{
				stream << "(" << (line + 1) << "): ";
			}

			stream << msg;

			m_msg = stream.str();
		}

		[[nodiscard]] const char *what() const noexcept override
		{
			return m_msg.c_str();
		}
	};

	namespace tools {

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

		std::string fixName(std::string name, bool mustEndWithSeparator = false);

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
	}
}

#ifdef NDEBUG
#define mt_ex(arg) mt::mt_exception(arg);
#else
#define mt_ex(arg) mt::mt_exception(arg, __FILE__, __LINE__);
#endif

#define throw_line(arg) throw mt_ex(arg);
