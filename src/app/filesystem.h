#pragma once

#include "mt.h"
#include "../lib/files.h"

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
		std::string path;						 ///< Path to the zip file
		unzFile zipFile = nullptr;				 ///< minizip zip instance
		std::unordered_set<std::string> files;	 ///< Files in the package (scanned for faster finds)
		std::unordered_set<std::string> folders; ///< Folders in the package (scanned for faster finds)
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

		[[nodiscard]] std::string getSource() const override;

	  private:
		std::vector<std::shared_ptr<FileSource>> sources;
	};
}
