#include "mt.h"
#include "io.h"

#include <vector>
#include <fstream>
#include <filesystem>


namespace mt::IO {
	bool FileExists(const std::string &path)
	{
		std::filesystem::path file(path);
		return exists(file);
	}

	bool FileSize(const std::string &path)
	{
		std::ifstream file(path, std::ifstream::ate | std::ifstream::binary);
		return file.tellg();
	}

	byte_buffer FileRead(const std::string &path)
	{
		std::ifstream infile(path, std::ios::ate | std::ios_base::binary);

		if (!infile.is_open())
		{
			return nullptr;
		}

		infile.seekg(0, std::ios::end);
		auto length = infile.tellg();
		infile.seekg(0, std::ios::beg);

		if (length > 0)
		{
			byte_buffer buffer = make_ref<std::vector<byte>>(1 + length);
			// fill the memory with zeroes
			std::fill(buffer->begin(), buffer->end(), 0);

			infile.read(reinterpret_cast<char *>(buffer->data()), length);
			infile.close();
			return buffer;
		}

		infile.close();
		return nullptr;
	}
}