#include "library.h"
#include "io.h"

#include <vector>
#include <fstream>
#include <filesystem>

namespace mt::io {
	bool FileExists(const std::string &path)
	{
		std::filesystem::path file(path);
		return exists(file);
	}

	long FileSize(const std::string &path)
	{
		std::error_code ec;
		if (const auto size = std::filesystem::file_size(path, ec); !ec)
		{
			return static_cast<long>(size);
		}
		else
		{
			return -1;
		}
	}

	byte_buffer FileRead(const std::string &path)
	{
		std::ifstream infile(path, std::ios::ate | std::ios_base::binary);

		if (!infile.is_open())
		{
			return nullptr;
		}

		infile.seekg(0, std::ios::end);
		long length = long(infile.tellg());
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
