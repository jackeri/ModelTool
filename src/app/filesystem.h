#pragma once

#include "mt.h"

#include <string>
#include <vector>
#include <unordered_set>

#include "unzip.h"

namespace mt::IO {

	struct MTFile {
		byte *data = nullptr;
		std::string name;
		std::string ext;
		size_t len = 0;

		MTFile() = default;

		~MTFile() = default;
	};

	class FileSource {
	public:
		FileSource();

		virtual ~FileSource() = default;

		virtual bool findFile(const std::string &name) = 0;

		virtual MTFile *loadFile(const std::string &name) = 0;

		[[nodiscard]] std::string getIdentifier() const
		{
			return identifier;
		}

	private:
		std::string identifier;
	};

	class MTPath : public FileSource {
	public:
		explicit MTPath(const std::string &path);

		~MTPath() override;

		bool findFile(const std::string &name) override;

		MTFile *loadFile(const std::string &name) override;

	private:
		std::string path;
	};

	class MTPackage : public FileSource {
	public:
		explicit MTPackage(const std::string &path);

		~MTPackage() override;

		bool findFile(const std::string &name) override;

		MTFile *loadFile(const std::string &name) override;

	private:
		unzFile zipFile;
		std::unordered_set<std::string> files;
	};

	class FileSystem : public FileSource {
	public:
		FileSystem();

		~FileSystem() override;

		bool addPath(const std::string &path);

		bool hasSource(FileSource &source);

		template<typename F>
		void addSource(std::shared_ptr<F> &source);

		void removeSource(FileSource &source);

		void clear();

		bool findFile(const std::string &name) override;

		MTFile *loadFile(const std::string &name) override;

	private:
		std::vector<std::shared_ptr<FileSource>> sources;
	};
}

