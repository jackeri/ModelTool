#pragma once

#include "library.h"
#include "files.h"

#include <map>
#include <unordered_map>

#include "unzip.h"

namespace mt::io {
	/*! Zip or pk3 file as a file source */
	class MTPackage : public FileSource {
	  public:
		explicit MTPackage(std::string path);

		~MTPackage() override;

		void loadPackage();

		bool findFile(const std::string &name) override;

		Ref<MTFile> loadFile(const std::string &name) override;

		FileList getFiles(const std::string &name) override;

		[[nodiscard]] std::string getSource() const override
		{
			return path;
		}

	  private:
		std::string path;											   ///< Path to the zip file
		unzFile zipFile = nullptr;									   ///< minizip zip instance
		std::unordered_set<std::string> files;						   ///< Files in the package (scanned for faster finds)
		std::unordered_set<std::string> folders;					   ///< Folders in the package (scanned for faster finds)
		std::unordered_map<std::string, std::string> lowercaseFiles{}; ///< lowercase name -> name in the package

		/**
		 * @param name file name to look for
		 * @return the name of the file in the package, empty if it is not there
		 */
		[[nodiscard]] std::string resolve(const std::string &name) const;
	};

	/*! File or folder of the virtual file system (base folders, pk3 files and pk3dir folders merged together) */
	struct VirtualEntry {
		std::string name{};					///< file or folder name
		bool isDirectory = false;			///< it is a folder
		std::vector<std::string> sources{}; ///< folders / pk3 files that have it, the file is loaded from the first one
	};

	/*! Class that wraps multiple other FileSources and unifies their usage to a single point of access */
	class FileSystem : public FileSource {
	  public:
		FileSystem();

		~FileSystem() override;

		/**
		 * Add a local filesystem path to the system
		 * @param path path to a folder or a file
		 * @return is the path valid
		 */
		bool addPath(const std::string &path);

		/**
		 * Checks if the FileSource is already added to the system
		 * @param source Source to check
		 * @return is the source already added
		 */
		bool hasSource(FileSource &source);

		/**
		 * Adds a new FileSource instance to the system
		 * @tparam F FileSource type
		 * @param source source to be added
		 */
		template<typename F>
		void addSource(std::shared_ptr<F> &source);

		/**
		 * Remove a FileSource from this system
		 * @param source source to be removed
		 */
		void removeSource(FileSource &source);

		/**
		 * Checks if the system has sources added
		 * @return does the system have sources
		 */
		bool hasSources();

		/**
		 * Clears this system from all file sources
		 */
		void clear();

		bool findFile(const std::string &name) override;

		Ref<MTFile> loadFile(const std::string &name) override;

		FileList getFiles(const std::string &name) override;

		/**
		 * Ignore the case of the names in every source (also the ones added later)
		 * @param value true to ignore the case
		 */
		void setCaseInsensitive(bool value) override;

		/**
		 * The files and folders of a folder of the virtual file system: every base folder, pk3 file and pk3dir folder
		 * merged together, folders first, sorted by name
		 * @param folder folder path relative to the root, empty for the root
		 * @return entries with the sources that have them
		 */
		std::vector<VirtualEntry> listFolder(const std::string &folder);

		[[nodiscard]] std::string getSource() const override;

	  private:
		std::vector<std::shared_ptr<FileSource>> sources;
	};
}
