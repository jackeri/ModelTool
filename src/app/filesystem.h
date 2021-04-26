#pragma once

#include "mt.h"

#include <string>
#include <vector>
#include <unordered_set>

#include "unzip.h"

namespace mt::IO {

	class MTFile {
	public:
		MTFile() = default;

		~MTFile() = default;

		[[nodiscard]] const char* c_str() const
		{
			return reinterpret_cast<char *>(data->data());
		}

		[[nodiscard]] std::string string() const
		{
			return std::string(c_str());
		}

		explicit operator const char*() const
		{
			return c_str();
		}

		explicit operator std::string() const
		{
			return string();
		}

		byte_buffer data = nullptr;
		std::string name;
		std::string ext;
		size_t len = 0;
	};

	class FileSource {
	public:
		FileSource();

		virtual ~FileSource() = default;

		virtual bool findFile(const std::string &name) = 0;

		virtual Ref<MTFile> loadFile(const std::string &name) = 0;

		[[nodiscard]] virtual std::string getSource() const = 0;

		[[nodiscard]] std::string getIdentifier() const
		{
			return identifier;
		}

	private:
		std::string identifier;
	};

	class MTPath : public FileSource {
	public:
		explicit MTPath(std::string path);

		~MTPath() override;

		bool findFile(const std::string &name) override;

		Ref<MTFile> loadFile(const std::string &name) override;

		[[nodiscard]] std::string getSource() const override
		{
			return path;
		}

	private:
		std::string path;
	};

	class MTPackage : public FileSource {
	public:
		explicit MTPackage(std::string path);

		~MTPackage() override;

		bool findFile(const std::string &name) override;

		Ref<MTFile> loadFile(const std::string &name) override;

		[[nodiscard]] std::string getSource() const override
		{
			return path;
		}

	private:
		std::string path;
		unzFile zipFile = nullptr;
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

		Ref<MTFile> loadFile(const std::string &name) override;

		std::string getSource() const override;

	private:
		std::vector<std::shared_ptr<FileSource>> sources;
	};
}

