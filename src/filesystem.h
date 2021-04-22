#pragma once

namespace mt {
	namespace IO {

		struct MTFile {
			byte *data = nullptr;
			std::string name;
			std::string ext;
			size_t len;

			MTFile();

			~MTFile();
		};

		class FileSource {
		public:
			virtual ~FileSource() = default;

			virtual bool findFile(const std::string &name) = 0;

			virtual MTFile *loadFile(const std::string &name) = 0;
		};

		class MTPath : FileSource {
		public:
			MTPath(std::string &path);
			~MTPath() override;


		};

		class filesystem {
		public:
			filesystem();

			~filesystem();

		private:
			std::vector<>
		};
	}
}

