#pragma once

#include "library.h"

#include <cstring>
#include <type_traits>

namespace mt::io {

	/*! Little-endian binary output buffer for the model writers */
	class BinaryWriter {
	  public:
		/**
		 * @return current size (the offset of the next write)
		 */
		[[nodiscard]] int32_t tell() const
		{
			if (data.size() > size_t(INT32_MAX))
			{
				throw mt_ex("The file is too large");
			}
			return int32_t(data.size());
		}

		/**
		 * Append a value
		 * @tparam T trivially copyable type
		 */
		template<typename T>
		void write(const T &value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter can only write trivially copyable types");
			const auto *bytes = reinterpret_cast<const byte *>(&value);
			data.insert(data.end(), bytes, bytes + sizeof(T));
		}

		/**
		 * Append the values of a vector
		 */
		template<typename T>
		void writeArray(const std::vector<T> &values)
		{
			for (const auto &value : values)
			{
				write(value);
			}
		}

		/**
		 * Overwrite a value written earlier
		 * @param offset offset of the value
		 * @param value new value
		 */
		template<typename T>
		void patch(int32_t offset, const T &value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter can only write trivially copyable types");
			if (offset < 0 || size_t(offset) + sizeof(T) > data.size())
			{
				throw mt_ex("Patch outside the written data");
			}
			std::memcpy(data.data() + offset, &value, sizeof(T));
		}

		/**
		 * Copy a string into a fixed size character field, the string needs to fit with its terminating null
		 * @param field character array
		 * @param value string to copy
		 * @param what name of the field for the error message
		 */
		template<size_t N>
		static void setString(char (&field)[N], const std::string &value, const std::string &what = "name")
		{
			if (value.size() >= N)
			{
				throw mt_ex("The " + what + " is too long (max " + std::to_string(N - 1) + " characters): " + value);
			}
			std::memset(field, 0, N);
			std::memcpy(field, value.data(), value.size());
		}

		/**
		 * Copy a string into a fixed size character field, cutting it if it does not fit
		 */
		template<size_t N>
		static void setStringTruncated(char (&field)[N], const std::string &value)
		{
			std::memset(field, 0, N);
			std::memcpy(field, value.data(), std::min(value.size(), N - 1));
		}

		std::vector<byte> data{}; ///< written bytes
	};

	/**
	 * Write bytes to a local file
	 * @param path path of the file
	 * @param data bytes to write
	 * @throws mt::mt_exception if the file can not be written
	 */
	inline void saveFile(const std::string &path, const std::vector<byte> &data)
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			throw mt_ex("Can not write the file: " + path);
		}
		out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
		if (!out.good())
		{
			throw mt_ex("Writing the file failed: " + path);
		}
	}

	/**
	 * Write text to a local file
	 */
	inline void saveFile(const std::string &path, const std::string &text)
	{
		saveFile(path, std::vector<byte>(text.begin(), text.end()));
	}
}
