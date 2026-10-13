#include "filesystem.h"

#include <filesystem>
#include <unordered_set>
#include <utility>

#include "library.h"
#include "mt_io.h"
#include "mt_path.h"

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
						lowercaseFiles.emplace(tools::lowercase(fileName), fileName);
					}

					// Many pk3 tools do not store folder entries, the folders of the files are there anyway
					std::string parent = fileName;
					for (auto slash = parent.find_last_of('/'); slash != std::string::npos && slash > 0; slash = parent.find_last_of('/'))
					{
						parent.resize(slash);
						folders.insert(parent);
					}
				}
			} while (unzGoToNextFile(zipFile) == UNZ_OK);

			unzGoToFirstFile(zipFile);
		}
	}

	std::string MTPackage::resolve(const std::string &name) const
	{
		std::string fixed = mt::tools::fixName(name);
		if (files.find(fixed) != files.end())
		{
			return fixed;
		}
		if (caseInsensitive)
		{
			auto found = lowercaseFiles.find(tools::lowercase(fixed));
			if (found != lowercaseFiles.end())
			{
				return found->second;
			}
		}
		return {};
	}

	bool MTPackage::findFile(const std::string &name)
	{
		return !resolve(name).empty();
	}

	Ref<MTFile> MTPackage::loadFile(const std::string &name)
	{
		std::string tmp = resolve(name);
		if (tmp.empty())
		{
			return nullptr;
		}

		// 1 = case sensitive, tmp is the name in the package
		if (UNZ_OK != unzLocateFile(zipFile, tmp.c_str(), 1))
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

		// One extra zeroed byte so text files are always null terminated
		output->data = make_ref<std::vector<byte>>(file_info.uncompressed_size + 1);
		std::fill(output->data->begin(), output->data->end(), 0);

		output->len = file_info.uncompressed_size;
		output->name = name;
		output->ext = std::filesystem::path(name).extension().string();

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
		auto startsWith = [&](const std::string &path) {
			return caseInsensitive ? tools::startsWith(tools::lowercase(path), tools::lowercase(fixedName))
								   : tools::startsWith(path, fixedName);
		};

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

			if (!startsWith(folder))
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

			if (!startsWith(file))
			{
				continue;
			}

			// "abuse" the filesystem api a bit
			std::filesystem::path tmp(file);

			records->push_back({file, tmp.filename().string(), tmp.extension().string(), false, this});
		}

		return records;
	}

	std::vector<std::string> MTPackage::allFiles()
	{
		return {files.begin(), files.end()};
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
			// Search order: the loose files first, then the pk3dir folders, then the pk3 files. Within the packages the
			// later names win like in the game (pak1.pk3 over pak0.pk3), so they are searched first
			sources.push_back(std::make_shared<MTPath>(absolute(sysPath).string()));

			// Do not scan the directory if it is supposed to be an unpacked pk3 (pk3dir)
			if (!tools::endsWith(absolute(sysPath).filename().string(), ".pk3dir"))
			{
				std::vector<std::filesystem::path> pk3dirs, pk3s;
				for (const auto &entry : std::filesystem::directory_iterator(path))
				{
					auto name = entry.path().filename().string();
					auto ext = entry.path().extension().string();

					if (entry.is_directory() && tools::endsWith(name, ".pk3dir"))
					{
						pk3dirs.push_back(absolute(entry.path()));
					}
					else if (entry.is_regular_file() && ext == ".pk3")
					{
						pk3s.push_back(absolute(entry.path()));
					}
				}

				auto laterFirst = [](const std::filesystem::path &a, const std::filesystem::path &b) {
					return a.filename().string() > b.filename().string();
				};
				std::sort(pk3dirs.begin(), pk3dirs.end(), laterFirst);
				std::sort(pk3s.begin(), pk3s.end(), laterFirst);
				for (const auto &folder : pk3dirs)
				{
					sources.push_back(std::make_shared<MTPath>(folder.string()));
				}
				for (const auto &package : pk3s)
				{
					sources.push_back(std::make_shared<MTPackage>(package.string()));
				}
			}
		}
		else if (is_regular_file(sysPath) && sysPath.extension() == ".pk3")
		{
			sources.push_back(std::make_shared<MTPackage>(absolute(sysPath).string()));
		}

		for (auto &source : sources)
		{
			source->setCaseInsensitive(caseInsensitive);
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
		source->setCaseInsensitive(caseInsensitive);
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
		if (std::any_of(sources.begin(), sources.end(), [&](auto &source) { return source->findFile(name); }))
		{
			return true;
		}

		// Files opened from anywhere on the disk (and their textures and levels of detail) use absolute paths
		std::error_code error;
		std::filesystem::path path(name);
		return path.is_absolute() && std::filesystem::is_regular_file(path, error);
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
		// FileRead adds a terminating null byte, which is not part of the file
		file->len = data->size() - 1;
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

		// Sort items by name but directories first
		std::sort(files->begin(), files->end(), [](const FileRecord &first, const FileRecord &second) {
			if (first.isDirectory == second.isDirectory)
			{
				return first.name < second.name;
			}
			return first.isDirectory > second.isDirectory;
		});

		// make sure we don't have duplicates (the same file or folder in several sources), needs the sorted list
		auto unique = std::unique(files->begin(), files->end(), [](const FileRecord &first, const FileRecord &second) {
			return (first.name == second.name && first.isDirectory == second.isDirectory);
		});

		files->erase(unique, files->end());

		return files;
	}

	void FileSystem::setCaseInsensitive(bool value)
	{
		FileSource::setCaseInsensitive(value);
		for (auto &source : sources)
		{
			source->setCaseInsensitive(value);
		}
	}

	std::vector<VirtualEntry> FileSystem::listFolder(const std::string &folder)
	{
		std::vector<VirtualEntry> entries;
		std::map<std::pair<bool, std::string>, size_t> index;

		// Sources are searched in order, so the first source of a file is the one it is loaded from
		for (auto &source : sources)
		{
			FileList found = source->getFiles(folder);
			for (const auto &record : *found)
			{
				if (record.name.empty())
				{
					continue;
				}
				// Names that only differ by case are the same file when the case is ignored
				auto key = std::make_pair(record.isDirectory, caseInsensitive ? tools::lowercase(record.name) : record.name);
				auto existing = index.find(key);
				if (existing == index.end())
				{
					index[key] = entries.size();
					entries.push_back({record.name, record.isDirectory, {source->getSource()}});
				}
				else if (entries[existing->second].sources.back() != source->getSource())
				{
					entries[existing->second].sources.push_back(source->getSource());
				}
			}
		}

		std::sort(entries.begin(), entries.end(), [](const VirtualEntry &first, const VirtualEntry &second) {
			if (first.isDirectory != second.isDirectory)
			{
				return first.isDirectory;
			}
			return first.name < second.name;
		});
		return entries;
	}

	std::vector<VirtualFile> FileSystem::listAllFiles()
	{
		std::vector<VirtualFile> list;
		std::unordered_set<std::string> seen;

		// The first source of a file is the one it is loaded from
		for (auto &source : sources)
		{
			for (auto &file : source->allFiles())
			{
				if (seen.insert(caseInsensitive ? tools::lowercase(file) : file).second)
				{
					list.push_back({std::move(file), source->getSource()});
				}
			}
		}

		std::sort(list.begin(), list.end(), [](const VirtualFile &first, const VirtualFile &second) { return first.path < second.path; });
		return list;
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
