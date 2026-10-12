#include "base_browser.h"
#include "state.h"
#include "model.h"

namespace mt {

	void BaseBrowser::show(const std::string &windowTitle, std::vector<std::string> fileExtensions,
						   std::function<void(const std::string &)> onSelect)
	{
		title = windowTitle;
		extensions = std::move(fileExtensions);
		callback = std::move(onSelect);
		open = true;
	}

	void BaseBrowser::render()
	{
		if (!open)
		{
			return;
		}

		auto &state = singleton<State>();

		ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (!ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!state.filesystem.hasSources())
		{
			ImGui::TextWrapped("No base folder. Use File > Base folders to add the game's mod folder (etmain).");
			ImGui::End();
			return;
		}

		ImGui::Text("/%s", folder.c_str());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##filter", "Filter", &filter);
		ImGui::Separator();

		std::string selectedFile;
		std::string enterFolder;
		bool goUp = false;

		ImGui::BeginChild("files", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders);
		if (!folder.empty() && ImGui::Selectable("[..]"))
		{
			goUp = true;
		}

		auto files = state.filesystem.getFiles(folder);
		for (const auto &record : *files)
		{
			if (!filter.empty() && record.name.find(filter) == std::string::npos && !record.isDirectory)
			{
				continue;
			}

			if (record.isDirectory)
			{
				if (ImGui::Selectable(("[" + record.name + "]").c_str()))
				{
					enterFolder = record.name;
				}
				continue;
			}

			const std::string ext = model::io::normalizeExtension(record.ext);
			if (!extensions.empty() && std::find(extensions.begin(), extensions.end(), ext) == extensions.end())
			{
				continue;
			}

			if (ImGui::Selectable(record.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
			{
				selectedFile = folder.empty() ? record.name : folder + "/" + record.name;
			}
		}
		ImGui::EndChild();

		ImGui::TextDisabled("Double click a file to open it");
		ImGui::End();

		if (goUp)
		{
			auto slash = folder.find_last_of('/');
			folder = slash == std::string::npos ? "" : folder.substr(0, slash);
		}
		else if (!enterFolder.empty())
		{
			folder = folder.empty() ? enterFolder : folder + "/" + enterFolder;
		}

		if (!selectedFile.empty())
		{
			open = false;
			callback(selectedFile);
		}
	}
}
