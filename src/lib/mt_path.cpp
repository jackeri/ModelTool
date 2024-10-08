#include "mt_path.h"
#include "io.h"

namespace mt::io {

	MTPath::MTPath(std::string path) : path(std::move(path))
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
		return FileExists(this->path + '/' + mt::tools::fixName(name));
	}

	Ref<MTFile> MTPath::loadFile(const std::string &name)
	{
		std::string tmp = mt::tools::fixName(name);

		std::filesystem::path sysPath(path);
		sysPath /= tmp;

		if (!exists(sysPath))
		{
			return nullptr;
		}

		byte_buffer data = FileRead(this->path + '/' + tmp);
		if (!data)
		{
			return nullptr;
		}

		auto file = make_ref<MTFile>();
		file->data = data;
		file->name = tmp;
		file->ext = sysPath.extension().string();

		return file;
	}

	FileList MTPath::getFiles(const std::string &name)
	{
		FileList files = make_ref_list<FileRecord>();

		std::filesystem::path sysPath(path);
		sysPath /= mt::tools::fixName(name);

		for (const auto &entry : std::filesystem::directory_iterator(sysPath))
		{
			if (!entry.is_directory() && !entry.is_regular_file())
			{
				continue;
			}

			// skip the pk3dirs
			if (entry.is_directory() && entry.path().extension().string() == ".pk3dir")
			{
				continue;
			}

			// skip the pk3
			if (entry.is_regular_file() && entry.path().extension().string() == ".pk3")
			{
				continue;
			}

			auto tmp = relative(entry.path(), {path});

			files->push_back({tmp.string(), tmp.filename().string(), tmp.extension().string(), entry.is_directory(), this});
		}

		return files;
	}
}
