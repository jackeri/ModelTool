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
		std::string path; ///< Local filesystem path which is used as a base
	};

}
