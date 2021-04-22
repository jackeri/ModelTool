#include "io.h"

#include <vector>
#include <fstream>


namespace mt {
	namespace IO {
		inline bool FileExists(const std::string &path)
		{
			std::ifstream file(path);
			return file.good();
		}

		inline bool FileSize(const std::string &path)
		{
			std::ifstream file(path, std::ifstream::ate | std::ifstream::binary);
			return file.tellg();
		}

		byte* FileRead(const std::string &path)
		{
			std::ifstream infile(path, std::ios::ate | std::ios_base::binary);

			infile.seekg(0, infile.end);
			size_t length = infile.tellg();
			infile.seekg(0, infile.beg);

			if (length > 0) {
				char* buffer = new char[length];
				infile.read(buffer, length);
				return reinterpret_cast<byte *>(buffer);
			}

			return nullptr;
		}
	}
}