#pragma once

#include "library.h"
#include "files.h"

namespace mt::io {

	/*! Local filesystem path as a FileSource */
	class MTPath : public FileSource {
	  public:
		explicit MTPath(std::string path);

		~MTPath() override;

		bool findFile(const std::string &name) override;

		Ref<MTFile> loadFile(const std::string &name) override;

		FileList getFiles(const std::string &name) override;

		[[nodiscard]] std::string getSource() const override
		{
			return path;
		}

	  private:
		/**
		 * Find a file or folder with the case rules of the source. The disk can ignore the case (macOS, Windows) or not
		 * (Linux), the names are checked so that both modes work the same on every system
		 * @param name relative path
		 * @param directory look for a folder instead of a file
		 * @return the path on the disk relative to the source, empty if there is no such file or folder
		 */
		[[nodiscard]] std::string resolve(const std::string &name, bool directory) const;

		std::string path; ///< Local filesystem path which is used as a base
	};

}
