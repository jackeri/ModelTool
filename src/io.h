#pragma once
#include "mt.h"
#include <string>

namespace mt {
	namespace IO {

		bool FileExists(const std::string &path);

		bool FileSize(const std::string &path);

		byte* FileRead(const std::string &path);
	}
}