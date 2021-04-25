#include "filesystem.h"
#include "io.h"

namespace mt::IO {

	MTPath::MTPath(std::string &path) : path(path)
	{
		// We only use '/' internally so sanitize the paths
		for (char &character : this->path)
		{
			if (character == '\\')
			{
				character = '/';
			}
		}
	}

	// Nothing to do here really
	MTPath::~MTPath() = default;

	bool MTPath::findFile(const std::string &name)
	{
		return FileExists(this->path + path);
	}

	MTFile *MTPath::loadFile(const std::string &name)
	{
		byte *data = FileRead(this->path + path);
		if (!data)
		{
			return nullptr;
		}

		auto *file = new MTFile;
		file->data = data;
		file->name = name;

		return file;
	}

	MTPackage::MTPackage(std::string &path)
	{

	}

	MTPackage::~MTPackage()
	{

	}

	bool MTPackage::findFile(const std::string &name)
	{
		return false;
	}

	MTFile *MTPackage::loadFile(const std::string &name)
	{
		return nullptr;
	}
}