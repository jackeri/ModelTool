#include "FileBrowser.h"

#include <utility>

namespace mt {
	void FileBrowser::show(const std::string &title, std::function<void(std::string)> callback, bool folderSelector)
	{
		if (browser)
		{
			browser = nullptr;
		}

		if (folderSelector)
		{
			browser = std::make_unique<ImGui::FileBrowser>(
					ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc);
		}
		else
		{
			browser = std::make_unique<ImGui::FileBrowser>(
					ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir |
					ImGuiFileBrowserFlags_CloseOnEsc);
		}

		this->browserCallback = std::move(callback);

		browser->SetTitle(title);
		browser->SetTypeFilters({});
		browser->Open();
	}

	void FileBrowser::setTypeFilters(const std::vector<std::string> &filters)
	{
		if (browser)
		{
			browser->SetTypeFilters(filters);
		}
	}

	void FileBrowser::handle()
	{
		if (!browser)
		{
			return;
		}

		browser->Display();

		if (browser->HasSelected())
		{
			browserCallback(browser->GetSelected().string());
			browser->ClearSelected();
		}
	}

	bool FileBrowser::isOpen()
	{
		return (browser && browser->IsOpened());
	}
}
