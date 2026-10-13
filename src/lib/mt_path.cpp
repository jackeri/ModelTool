#include "mt_path.h"
#include "mt_io.h"
#include <filesystem>

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

	std::string MTPath::resolve(const std::string &name, bool directory) const
	{
		std::string fixed = mt::tools::fixName(name);
		while (!fixed.empty() && fixed.back() == '/')
		{
			fixed.pop_back();
		}

		std::error_code error;
		auto isWanted = [&](const std::filesystem::path &candidate) {
			return directory ? std::filesystem::is_directory(candidate, error) : std::filesystem::is_regular_file(candidate, error);
		};

		if (fixed.empty())
		{
			return directory ? fixed : std::string();
		}

		// Fast path: the name works as it is (ignoring the case, the disk's own rules are fine)
		const bool exact = isWanted(std::filesystem::path(path) / fixed);
		if (caseInsensitive && exact)
		{
			return fixed;
		}
		if (!caseInsensitive && !exact)
		{
			return {};
		}

		// Match every part of the path against the folder entries
		std::filesystem::path current(path);
		std::string resolved;
		size_t start = 0;
		while (start <= fixed.size())
		{
			size_t end = fixed.find('/', start);
			const std::string part = fixed.substr(start, end == std::string::npos ? std::string::npos : end - start);
			const std::string lowerPart = tools::lowercase(part);

			std::string match;
			for (const auto &entry : std::filesystem::directory_iterator(current, error))
			{
				const std::string entryName = entry.path().filename().string();
				if (entryName == part)
				{
					match = entryName;
					break;
				}
				if (caseInsensitive && match.empty() && tools::lowercase(entryName) == lowerPart)
				{
					match = entryName;
				}
			}
			if (match.empty())
			{
				return {};
			}

			current /= match;
			resolved = resolved.empty() ? match : resolved + "/" + match;
			if (end == std::string::npos)
			{
				break;
			}
			start = end + 1;
		}

		return isWanted(current) ? resolved : std::string();
	}

	bool MTPath::findFile(const std::string &name)
	{
		return !resolve(name, false).empty();
	}

	Ref<MTFile> MTPath::loadFile(const std::string &name)
	{
		std::string tmp = resolve(name, false);
		if (tmp.empty())
		{
			return nullptr;
		}

		std::filesystem::path sysPath(path);
		sysPath /= tmp;

		byte_buffer data = FileRead(this->path + '/' + tmp);
		if (!data)
		{
			return nullptr;
		}

		auto file = make_ref<MTFile>();
		file->data = data;
		// FileRead adds a terminating null byte, which is not part of the file
		file->len = data->size() - 1;
		file->name = tmp;
		file->ext = sysPath.extension().string();

		return file;
	}

	FileList MTPath::getFiles(const std::string &name)
	{
		FileList files = make_ref_list<FileRecord>();

		// The folder can exist in other sources only (another pk3 or pk3dir)
		std::error_code error;
		const std::string folder = resolve(name, true);
		std::filesystem::path sysPath(path);
		if (!folder.empty())
		{
			sysPath /= folder;
		}
		if ((folder.empty() && !mt::tools::fixName(name).empty()) || !std::filesystem::is_directory(sysPath, error))
		{
			return files;
		}

		for (const auto &entry : std::filesystem::directory_iterator(sysPath, error))
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

	std::vector<std::string> MTPath::allFiles()
	{
		std::vector<std::string> list;
		std::error_code error;
		std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, error);
		for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
		{
			const auto &entry = *it;
			// pk3dir folders and pk3 files are sources of their own
			if (entry.is_directory(error) && entry.path().extension().string() == ".pk3dir")
			{
				it.disable_recursion_pending();
				continue;
			}
			if (!entry.is_regular_file(error) || entry.path().extension().string() == ".pk3")
			{
				continue;
			}
			list.push_back(std::filesystem::relative(entry.path(), path, error).generic_string());
		}
		return list;
	}
}
