#pragma once

#include "library.h"
#include "types.h"
#include <_stdlib.h>

namespace mt::io {

	/**
	 * Checks if a file exists in the local filesystem
	 * @param path absolute or relative path to a file
	 * @return does the file exist
	 */
	bool FileExists(const std::string &path);

	/**
	 * Checks the size of a file
	 * @param path absolute or relative path to a file
	 * @return file size or -1 if the file does not exist
	 */
	long FileSize(const std::string &path);

	/**
	 * Reads a file to memory
	 * @param path absolute or relative path to a file
	 * @return file buffer
	 */
	byte_buffer FileRead(const std::string &path);

	template<typename T>
	inline i32 write(std::ostream &os, const T &data)
	{
		os.write(reinterpret_cast<const char *>(&data), sizeof(T));
		return sizeof(T);
	}

	template<typename T>
	inline i32 write(std::ostream &os, const std::vector<T> &data)
	{
		i32 size = 0;
		for (const auto &item : data)
		{
			size += write(os, item);
		}
		return size;
	}

	template<typename T>
	i32 read(std::istream &is, T &data)
	{
		is.read(reinterpret_cast<char *>(&data), sizeof(T));
		return sizeof(T);
	}

	template<typename T>
	i32 read(std::istream &is, std::vector<T> &data)
	{
		// i32 size = 0;
		// for (auto &item : data)
		// {
		// 	size += read(is, item);
		// }
		// return size;
		// FIXME: implement
		return -1;
	}
}
