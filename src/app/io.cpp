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

		infile.seekg(0, infile.end);
		auto length = infile.tellg();
		infile.seekg(0, infile.beg);

		if (length > 0)
		{
			byte_buffer buffer = std::make_shared<std::vector<byte>>(length);
			infile.read(reinterpret_cast<char *>(buffer->data()), length);
			return buffer;
		}

		return nullptr;
	}
}