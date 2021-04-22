#pragma once

#include "mt.h"

#include <string>
#include <vector>

namespace mt {
	namespace IO {

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
			virtual ~FileSource() = default;

			virtual bool findFile(const std::string &name) = 0;

			virtual MTFile *loadFile(const std::string &name) = 0;
		};

		class MTPath : FileSource {
		public:
			explicit MTPath(std::string &path);

			~MTPath() override;

			bool findFile(const int &name) override;

			MTFile *loadFile(const int &name) override;
		};

		class MTPackage : FileSource {
		public:
			explicit MTPackage(std::string &path);

			~MTPackage() override;

			bool findFile(const int &name) override;

			MTFile *loadFile(const int &name) override;
		};

		class FileSystem {
		public:
			FileSystem();

			~FileSystem();

		private:
			std::vector<FileSource> sources;
		};
	}
}

