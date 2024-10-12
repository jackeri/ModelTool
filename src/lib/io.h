#pragma once

#include "library.h"
#include "types.h"

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
	i32 write(std::ostream &os, const T &data)
	{
		os.write(reinterpret_cast<const char *>(&data), sizeof(T));
		return sizeof(T);
	}
}
