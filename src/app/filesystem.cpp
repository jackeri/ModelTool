#include "filesystem.h"
#include "tools.h"

#include <filesystem>
#include <utility>

#include "../lib/library.h"
#include "../lib/io.h"
#include "../lib/mt_path.h"

namespace mt::io {

	MTPackage::MTPackage(std::string path) : path(std::move(path))
	{
		loadPackage();
	}

	MTPackage::~MTPackage()
	{
		unzClose(zipFile);
		files = std::unordered_set<std::string>();
	}

	void MTPackage::loadPackage()
	{
		zipFile = unzOpen(path.c_str());

		if (!zipFile)
		{
			throw mt_ex("Invalid package");
		}

		int err = unzGoToFirstFile(zipFile);
		if (UNZ_OK == err)
		{
			do
			{
				unz_file_info fileInfo;
				char fileName[MAX_PATH];
				memset(&fileInfo, 0, sizeof(unz_file_info));
				if (unzGetCurrentFileInfo(zipFile, &fileInfo, fileName, MAX_PATH, nullptr, 0, nullptr, 0) == UNZ_OK)
				{
					size_t len = strlen(fileName);

					if (fileName[len - 1] == '/')
					{
						// remove the last separator
						fileName[len - 1] = '\0';
						folders.insert(fileName);
					}
					else
					{
						files.insert(fileName);
					}
				}
			} while (unzGoToNextFile(zipFile) == UNZ_OK);

			unzGoToFirstFile(zipFile);
		}
	}

	bool MTPackage::findFile(const std::string &name)
	{
		return (files.find(mt::tools::fixName(name)) != files.end());
	}

	Ref<MTFile> MTPackage::loadFile(const std::string &name)
	{
		std::string tmp = mt::tools::fixName(name);

		if (!findFile(tmp))
		{
			return nullptr;
		}

		if (UNZ_OK != unzLocateFile(zipFile, tmp.c_str(), false))
		{
			return nullptr;
		}

		// Get info about current file.
		unz_file_info file_info;
		char filename[MAX_PATH];
		if (unzGetCurrentFileInfo(zipFile, &file_info, filename, MAX_PATH, nullptr, 0, nullptr, 0) != UNZ_OK)
		{
			return nullptr;
		}

		if (unzOpenCurrentFile(zipFile) != UNZ_OK)
		{
			return nullptr;
		}

		auto output = make_ref<MTFile>();

		output->data = make_ref<std::vector<byte>>(file_info.uncompressed_size);
		std::fill(output->data->begin(), output->data->end(), 0);

		output->len = file_info.uncompressed_size;
		output->name = name;

		int result = unzReadCurrentFile(zipFile, output->data->data(), file_info.uncompressed_size);

		if (result > 0)
		{
			return output;
		}

		return nullptr;
	}

	FileList MTPackage::getFiles(const std::string &name)
	{
		static auto charFinder = [](const char &val) { return val == '/'; };

		FileList records = make_ref_list<FileRecord>();
		unsigned int separators = 0;
		std::string fixedName = mt::tools::fixName(name, true);

		if (!fixedName.empty())
		{
			separators = std::count_if(fixedName.begin(), fixedName.end(), charFinder);
		}

		for (auto &folder : folders)
		{
			unsigned int count = std::count_if(folder.begin(), folder.end(), charFinder);

			if (count != separators)
			{
				continue;
			}

			if (!tools::startsWith(folder, fixedName))
			{
				continue;
			}

			// "abuse" the filesystem api a bit
			std::filesystem::path tmp(folder);

			records->push_back({folder, tmp.filename().string(), {}, true, this});
		}

		for (auto &file : files)
		{
			unsigned int count = std::count_if(file.begin(), file.end(), charFinder);

			if (count != separators)
			{
				continue;
			}

			if (!tools::startsWith(file, fixedName))
			{
				continue;
			}

			// "abuse" the filesystem api a bit
			std::filesystem::path tmp(file);

			records->push_back({file, tmp.filename().string(), tmp.extension().string(), false, this});
		}

		return records;
	}

	FileSystem::FileSystem() : FileSource() {}

	FileSystem::~FileSystem()
	{
		clear();
	}

	bool FileSystem::addPath(const std::string &path)
	{
		std::filesystem::path sysPath(path);

		if (!exists(sysPath))
		{
			return false;
		}

		if (is_directory(sysPath))
		{
			// Do not scan the directory if it is supposed to be an unpacked pk3 (pk3dir)
			if (!tools::endsWith(absolute(sysPath).filename().string(), ".pk3dir"))
			{
				for (const auto &entry : std::filesystem::directory_iterator(path))
				{
					auto name = entry.path().filename().string();
					auto ext = entry.path().extension().string();

					// Check if directory, ignore if not a PK3Dir
					if (entry.is_directory() && tools::endsWith(name, ".pk3dir"))
					{
						sources.push_back(std::make_shared<MTPath>(absolute(entry.path()).string()));
					}
					else if (entry.is_regular_file() && ext == ".pk3")
					{
						sources.push_back(std::make_shared<MTPackage>(absolute(entry.path()).string()));
					}
				}
			}

			sources.push_back(std::make_shared<MTPath>(absolute(sysPath).string()));
		}
		else if (is_regular_file(sysPath) && sysPath.extension() == ".pk3")
		{
			sources.push_back(std::make_shared<MTPackage>(absolute(sysPath).string()));
		}

		return true;
	}

	bool FileSystem::hasSource(FileSource &source)
	{
		return std::find_if(sources.begin(), sources.end(),
							[&](auto &tempSource) { return source.getIdentifier() == tempSource->getIdentifier(); })
			   != sources.end();
	}

	template<typename F>
	void FileSystem::addSource(std::shared_ptr<F> &source)
	{
		sources.emplace_back(source);
	}

	void FileSystem::removeSource(FileSource &source)
	{
		if (!hasSource(source))
		{
			return;
		}

		auto point = std::find_if(sources.begin(), sources.end(),
								  [&](auto &tempSource) { return source.getIdentifier() == tempSource->getIdentifier(); });

		sources.erase(point);
	}

	bool FileSystem::hasSources()
	{
		return !sources.empty();
	}

	void FileSystem::clear()
	{
		sources.clear();
	}

	bool FileSystem::findFile(const std::string &name)
	{
		return std::any_of(sources.begin(), sources.end(), [&](auto &source) { return source->findFile(name); });
	}

	Ref<MTFile> FileSystem::loadFile(const std::string &name)
	{
		for (auto &source : sources)
		{
			if (source->findFile(name))
			{
				return source->loadFile(name);
			}
		}

		std::filesystem::path sysPath(name);
		if (!exists(sysPath))
		{
			return nullptr;
		}

		byte_buffer data = FileRead(sysPath.string());
		if (!data)
		{
			return nullptr;
		}

		auto file = make_ref<MTFile>();
		file->data = data;
		file->name = name;
		file->ext = sysPath.extension().string();

		return file;
	}

	FileList FileSystem::getFiles(const std::string &name)
	{
		FileList files = make_ref_list<FileRecord>();

		for (auto &source : sources)
		{
			FileList found = source->getFiles(name);
			files->insert(files->end(), found->begin(), found->end());
		}

		// make sure we don't have duplicates
		auto unique = std::unique(files->begin(), files->end(), [](const FileRecord &first, const FileRecord &second) {
			return (first.name == second.name && first.isDirectory == second.isDirectory);
		});

		files->erase(unique, files->end());

		// Sort items by name but directories first
		std::sort(files->begin(), files->end(), [](const FileRecord &first, const FileRecord &second) {
			if (first.isDirectory == second.isDirectory)
			{
				return first.name < second.name;
			}
			return first.isDirectory > second.isDirectory;
		});

		return files;
	}

	std::string FileSystem::getSource() const
	{
		std::string output;
		for (auto &source : sources)
		{
			output.append(source->getSource());
			output.append("\n");
		}

		return output;
	}
}
