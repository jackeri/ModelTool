#pragma once

#include "mt.h"
#include <imfilebrowser.h>

namespace mt {
	class FileBrowser {
	public:
		FileBrowser() = default;

		~FileBrowser() = default;

		void show(const std::string &title, std::function<void(std::string)> callback, bool folderSelector = false);

		void setTypeFilters(const std::vector<std::string> &);

		bool isOpen();

		void handle();

	private:
		std::unique_ptr<ImGui::FileBrowser> browser = nullptr;
		std::function<void(std::string)> browserCallback{};
	};
}
