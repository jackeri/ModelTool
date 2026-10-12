#include "base_browser.h"
#include "icons_font_awesome.h"
#include "model.h"
#include "state.h"
#include "fuzzy.h"

#include <filesystem>

namespace mt {

	namespace {
		std::string lowercase(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}

		/**
		 * Short name of a source: the pk3 file or pk3dir / base folder name
		 */
		std::string sourceName(const std::string &source)
		{
			std::filesystem::path path(source);
			std::string name = path.filename().string();
			return name.empty() ? path.parent_path().filename().string() : name;
		}

		std::string parentOf(const std::string &path)
		{
			auto slash = path.find_last_of('/');
			return slash == std::string::npos ? std::string() : path.substr(0, slash);
		}
	}

	void BaseBrowser::show(const std::string &windowTitle, std::vector<std::string> fileExtensions,
						   std::function<void(const std::string &)> onSelect)
	{
		title = windowTitle;
		extensions = std::move(fileExtensions);
		callback = std::move(onSelect);
		allTypes = extensions.empty();
		search.clear();
		searchedText.clear();
		allFilesRead = false;
		focusSearch = true;
		open = true;
		openPopup = true;

		// Start next to the open model when it came from the base folders
		auto &state = singleton<State>();
		if (!state.modelPath.empty() && !std::filesystem::path(state.modelPath).is_absolute() && state.filesystem.findFile(state.modelPath))
		{
			folder = parentOf(state.modelPath);
		}
		enterFolder(folder);
	}

	void BaseBrowser::enterFolder(const std::string &path)
	{
		folder = path;
		while (!folder.empty() && folder.back() == '/')
		{
			folder.pop_back();
		}
		selected.clear();
		pathInput.clear();
		refresh();

		// A folder that is gone (other base folders) starts over at the root
		if (entries.empty() && !folder.empty())
		{
			folder.clear();
			refresh();
		}
	}

	void BaseBrowser::refresh()
	{
		entries = singleton<State>().filesystem.listFolder(folder);
		allFilesRead = false;
		searchedText.clear();
	}

	void BaseBrowser::updateSearch()
	{
		if (search == searchedText && allTypes == searchedAllTypes && allFilesRead)
		{
			return;
		}

		if (!allFilesRead)
		{
			allFiles = singleton<State>().filesystem.listAllFiles();
			allFilesRead = true;
			searchedText.clear();
		}

		// The type filter limits the files that are searched
		if (allTypes != searchedAllTypes || searchedText.empty() || candidates.empty())
		{
			candidates.clear();
			for (size_t i = 0; i < allFiles.size(); i++)
			{
				if (typeShown(allFiles[i].path))
				{
					candidates.push_back(i);
				}
			}
		}

		std::vector<std::string> paths;
		paths.reserve(candidates.size());
		for (size_t index : candidates)
		{
			paths.push_back(allFiles[index].path);
		}

		// Only the best ones are drawn, the count tells how many there are
		results = fuzzyFind(search, paths);
		matchCount = results.size();
		constexpr size_t MAX_SHOWN = 500;
		if (results.size() > MAX_SHOWN)
		{
			results.resize(MAX_SHOWN);
		}

		searchedText = search;
		searchedAllTypes = allTypes;
		selectedResult = 0;
		scrollToResult = true;
		pathInput = results.empty() ? std::string() : allFiles[candidates[results.front().index]].path;
	}

	std::string BaseBrowser::drawSearchResults()
	{
		updateSearch();

		// Up and down move the selection while typing
		const int count = int(results.size());
		if (count > 0 && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
		{
			selectedResult = std::min(selectedResult + 1, count - 1);
			scrollToResult = true;
		}
		if (count > 0 && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
		{
			selectedResult = std::max(selectedResult - 1, 0);
			scrollToResult = true;
		}
		if (count > 0)
		{
			pathInput = allFiles[candidates[results[size_t(selectedResult)].index]].path;
		}

		std::string openFile;
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
									  | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
		const float bottom
			= ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing();
		if (ImGui::BeginTable("results", 3, flags, {0, -bottom}))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 4.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.8f);
			ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableHeadersRow();

			const ImVec4 highlight = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
			for (int row = 0; row < count; row++)
			{
				const auto &result = results[size_t(row)];
				const io::VirtualFile &file = allFiles[candidates[result.index]];

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(row);
				const bool isSelected = row == selectedResult;
				if (ImGui::Selectable("##result", isSelected,
									  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick
										  | ImGuiSelectableFlags_AllowOverlap))
				{
					selectedResult = row;
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						openFile = file.path;
					}
				}
				if (isSelected && scrollToResult)
				{
					ImGui::SetScrollHereY();
					scrollToResult = false;
				}

				// The path with the matched characters highlighted
				ImGui::SameLine(0, 0);
				ImGui::TextUnformatted(ICON_FA_FILE_O " ");
				size_t next = 0;
				const auto &positions = result.match.positions;
				size_t start = 0;
				while (start < file.path.size())
				{
					const bool matched = next < positions.size() && positions[next] == start;
					size_t end = start;
					if (matched)
					{
						while (next < positions.size() && positions[next] == end)
						{
							next++;
							end++;
						}
					}
					else
					{
						end = next < positions.size() ? positions[next] : file.path.size();
					}
					ImGui::SameLine(0, 0);
					const std::string part = file.path.substr(start, end - start);
					if (matched)
					{
						ImGui::TextColored(highlight, "%s", part.c_str());
					}
					else
					{
						ImGui::TextUnformatted(part.c_str());
					}
					start = end;
				}

				ImGui::TableNextColumn();
				std::string ext = model::io::normalizeExtension(file.path);
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::toupper(c)); });
				ImGui::TextUnformatted(ext.empty() ? "File" : ext.substr(1).c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(sourceName(file.source).c_str());
				ImGui::SetItemTooltip("%s", file.source.c_str());
				ImGui::PopID();
			}

			if (count == 0)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("No file matches");
			}
			ImGui::EndTable();
		}

		if (matchCount > results.size())
		{
			ImGui::TextDisabled("Showing the best %zu of %zu matches in %zu files", results.size(), matchCount, candidates.size());
		}
		else
		{
			ImGui::TextDisabled("%zu matches in %zu files", matchCount, candidates.size());
		}
		return openFile;
	}

	std::string BaseBrowser::pathOf(const std::string &name) const
	{
		return folder.empty() ? name : folder + "/" + name;
	}

	bool BaseBrowser::typeShown(const std::string &name) const
	{
		if (allTypes || extensions.empty())
		{
			return true;
		}
		return std::find(extensions.begin(), extensions.end(), model::io::normalizeExtension(name)) != extensions.end();
	}

	bool BaseBrowser::accept(const std::string &path)
	{
		std::string clean = tools::fixName(path);
		while (!clean.empty() && clean.back() == '/')
		{
			clean.pop_back();
		}
		if (clean.empty())
		{
			return false;
		}

		auto &files = singleton<State>().filesystem;
		if (files.findFile(clean))
		{
			open = false;
			ImGui::CloseCurrentPopup();
			callback(clean);
			return true;
		}

		// A typed folder path is entered
		if (!files.listFolder(clean).empty())
		{
			enterFolder(clean);
		}
		return false;
	}

	void BaseBrowser::drawPathBar()
	{
		ImGui::BeginDisabled(folder.empty());
		if (ImGui::Button(ICON_FA_LEVEL_UP))
		{
			enterFolder(parentOf(folder));
		}
		ImGui::SetItemTooltip("Up (Backspace)");
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_REFRESH))
		{
			refresh();
		}
		ImGui::SetItemTooltip("Read the folder again");
		ImGui::SameLine();

		// Root and every folder of the path as buttons
		std::string target;
		bool clicked = false;
		if (ImGui::SmallButton(ICON_FA_HOME " /"))
		{
			clicked = true;
		}
		ImGui::SetItemTooltip("Root: the base folders with their pk3 files and pk3dir folders");
		std::string path;
		size_t start = 0;
		int id = 0;
		while (start < folder.size())
		{
			size_t end = folder.find('/', start);
			std::string part = folder.substr(start, end == std::string::npos ? std::string::npos : end - start);
			path = path.empty() ? part : path + "/" + part;
			ImGui::SameLine(0, 2);
			ImGui::TextDisabled("/");
			ImGui::SameLine(0, 2);
			ImGui::PushID(id++);
			if (ImGui::SmallButton(part.c_str()))
			{
				clicked = true;
				target = path;
			}
			ImGui::PopID();
			if (end == std::string::npos)
			{
				break;
			}
			start = end + 1;
		}
		if (clicked)
		{
			enterFolder(target);
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		if (focusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			focusSearch = false;
		}
		searchSubmitted = ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Search all files (fuzzy), like grunt head md3", &search,
												   ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SetItemTooltip("Searches every folder, pk3 and pk3dir from the root. Up / Down pick a result, Enter opens it");
	}

	std::string BaseBrowser::drawTable()
	{
		std::string openFile;
		std::string openFolder;

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
									  | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
		const float bottom = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;
		if (!ImGui::BeginTable("files", 3, flags, {0, -bottom}))
		{
			return {};
		}
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 1.6f);
		ImGui::TableHeadersRow();

		int shown = 0;
		for (const auto &entry : entries)
		{
			if (!entry.isDirectory && !typeShown(entry.name))
			{
				continue;
			}
			shown++;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(entry.isDirectory ? "dir" : "file");
			ImGui::PushID(entry.name.c_str());

			const std::string label = std::string(entry.isDirectory ? ICON_FA_FOLDER : ICON_FA_FILE_O) + " " + entry.name;
			const bool isSelected = selected == entry.name;
			if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
			{
				selected = entry.name;
				pathInput = entry.isDirectory ? std::string() : pathOf(entry.name);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					(entry.isDirectory ? openFolder : openFile) = pathOf(entry.name);
				}
			}

			ImGui::TableNextColumn();
			if (entry.isDirectory)
			{
				ImGui::TextDisabled("Folder");
			}
			else
			{
				std::string ext = model::io::normalizeExtension(entry.name);
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::toupper(c)); });
				ImGui::TextUnformatted(ext.empty() ? "File" : ext.substr(1).c_str());
			}

			// Where it comes from, files are loaded from the first source
			ImGui::TableNextColumn();
			if (!entry.sources.empty())
			{
				std::string source = sourceName(entry.sources.front());
				if (entry.sources.size() > 1)
				{
					source += " +" + std::to_string(entry.sources.size() - 1);
				}
				ImGui::TextUnformatted(source.c_str());
				if (ImGui::BeginItemTooltip())
				{
					ImGui::TextDisabled(entry.isDirectory || entry.sources.size() == 1 ? "In:" : "Loaded from the first one:");
					for (const auto &path : entry.sources)
					{
						ImGui::TextUnformatted(path.c_str());
					}
					ImGui::EndTooltip();
				}
			}

			ImGui::PopID();
			ImGui::PopID();
		}

		if (shown == 0)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(entries.empty() ? "Empty folder" : "No files of this type");
		}
		ImGui::EndTable();

		if (!openFolder.empty())
		{
			enterFolder(openFolder);
		}
		return openFile;
	}

	void BaseBrowser::render()
	{
		if (!open)
		{
			return;
		}

		auto &state = singleton<State>();
		const std::string popupId = title + "###baseBrowser";
		if (openPopup)
		{
			ImGui::OpenPopup(popupId.c_str());
			openPopup = false;
		}

		ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 52, ImGui::GetFontSize() * 34), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (!ImGui::BeginPopupModal(popupId.c_str(), &open))
		{
			open = false;
			return;
		}

		if (!state.filesystem.hasSources())
		{
			ImGui::TextWrapped("No base folder. Use File > Base folders to add the game's mod folder (etmain).");
			if (ImGui::Button("Close"))
			{
				open = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
			return;
		}

		drawPathBar();
		const bool searching = !search.empty();
		if (!searching && !searchedText.empty())
		{
			// Back to browsing the folders
			searchedText.clear();
			pathInput.clear();
			selected.clear();
		}
		std::string openFile = searching ? drawSearchResults() : drawTable();

		// Selected file (can be typed), the type filter and the buttons
		const float buttons = ImGui::CalcTextSize("Cancel").x + ImGui::CalcTextSize("Open").x + ImGui::GetStyle().FramePadding.x * 4
							  + ImGui::GetStyle().ItemSpacing.x * 2;
		ImGui::SetNextItemWidth(-buttons);
		const bool enter
			= ImGui::InputTextWithHint("##path", "File (relative to the root)", &pathInput, ImGuiInputTextFlags_EnterReturnsTrue);

		std::string typeLabel = "All files";
		if (!extensions.empty())
		{
			std::string list;
			for (const auto &ext : extensions)
			{
				list += (list.empty() ? "*" : ", *") + ext;
			}
			typeLabel = "Supported (" + list + ")";
		}
		ImGui::SetNextItemWidth(-buttons);
		if (ImGui::BeginCombo("##types", allTypes ? "All files" : typeLabel.c_str()))
		{
			if (!extensions.empty() && ImGui::Selectable(typeLabel.c_str(), !allTypes))
			{
				allTypes = false;
			}
			if (ImGui::Selectable("All files", allTypes))
			{
				allTypes = true;
			}
			ImGui::EndCombo();
		}

		// The buttons sit next to the two fields
		ImGui::SetCursorPos({ImGui::GetWindowContentRegionMax().x - buttons + ImGui::GetStyle().ItemSpacing.x,
							 ImGui::GetCursorPosY() - ImGui::GetFrameHeightWithSpacing()});
		ImGui::BeginDisabled(pathInput.empty());
		const bool openClicked = ImGui::Button("Open");
		ImGui::EndDisabled();
		ImGui::SameLine();
		const bool cancel = ImGui::Button("Cancel");

		// Keyboard: Backspace goes up, Enter opens the selected file, Escape cancels
		const bool typing = ImGui::GetIO().WantTextInput;
		if (!typing && !folder.empty() && ImGui::IsKeyPressed(ImGuiKey_Backspace))
		{
			enterFolder(parentOf(folder));
		}
		const bool enterKey = !typing && ImGui::IsKeyPressed(ImGuiKey_Enter);

		if (cancel || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			open = false;
			ImGui::CloseCurrentPopup();
		}
		else if (!openFile.empty())
		{
			accept(openFile);
		}
		else if (searchSubmitted && searching)
		{
			accept(pathInput);
		}
		else if (openClicked || enter || enterKey)
		{
			if (enterKey && pathInput.empty() && !selected.empty())
			{
				enterFolder(pathOf(selected));
			}
			else
			{
				accept(pathInput);
			}
		}

		ImGui::EndPopup();
	}
}
