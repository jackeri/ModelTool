#include "log_panel.h"

namespace mt {

	std::shared_ptr<LogBuffer> LogBuffer::instance()
	{
		static auto buffer = std::make_shared<LogBuffer>();
		return buffer;
	}

	std::vector<LogBuffer::Entry> LogBuffer::entries()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return {buffer.begin(), buffer.end()};
	}

	void LogBuffer::clearEntries()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		buffer.clear();
	}

	size_t LogBuffer::counter()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return total;
	}

	void LogBuffer::sink_it_(const spdlog::details::log_msg &message)
	{
		buffer.push_back({message.level, std::string(message.payload.begin(), message.payload.end())});
		if (buffer.size() > MAX_ENTRIES)
		{
			buffer.pop_front();
		}
		total++;
	}

	void LogPanel::render(bool &open)
	{
		if (!open)
		{
			return;
		}

		if (!ImGui::Begin("Log", &open))
		{
			ImGui::End();
			return;
		}

		auto buffer = LogBuffer::instance();

		const char *levels[] = {"Trace", "Debug", "Info", "Warning", "Error"};
		ImGui::SetNextItemWidth(100);
		ImGui::Combo("##level", &minimumLevel, levels, IM_ARRAYSIZE(levels));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200);
		ImGui::InputTextWithHint("##filter", "Filter", &filter);
		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &autoScroll);
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			buffer->clearEntries();
		}
		ImGui::Separator();

		ImGui::BeginChild("messages", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
		for (const auto &entry : buffer->entries())
		{
			if (int(entry.level) < minimumLevel || (!filter.empty() && entry.text.find(filter) == std::string::npos))
			{
				continue;
			}

			ImVec4 color{0.85f, 0.85f, 0.85f, 1.0f};
			if (entry.level >= spdlog::level::err)
			{
				color = {1.0f, 0.4f, 0.4f, 1.0f};
			}
			else if (entry.level == spdlog::level::warn)
			{
				color = {1.0f, 0.8f, 0.3f, 1.0f};
			}
			else if (entry.level <= spdlog::level::debug)
			{
				color = {0.55f, 0.55f, 0.55f, 1.0f};
			}
			ImGui::TextColored(color, "%s", entry.text.c_str());
		}

		const size_t counter = buffer->counter();
		if (autoScroll && counter != lastCounter)
		{
			ImGui::SetScrollHereY(1.0f);
		}
		lastCounter = counter;

		ImGui::EndChild();
		ImGui::End();
	}
}
