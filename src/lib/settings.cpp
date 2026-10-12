#include "settings.h"
#include "script_stream.h"

#include <cstdlib>
#include <filesystem>
#include <iomanip>

namespace mt {

	void AppSettings::addRecentFile(const std::string &path)
	{
		recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), path), recentFiles.end());
		recentFiles.insert(recentFiles.begin(), path);
		if (recentFiles.size() > MAX_RECENT_FILES)
		{
			recentFiles.resize(MAX_RECENT_FILES);
		}
	}

	static std::string quote(const std::string &value)
	{
		std::string out = "\"";
		for (char c : value)
		{
			if (c == '"' || c == '\n')
			{
				throw mt_ex("Settings values can not contain quotes or line breaks: " + value);
			}
			out += c;
		}
		return out + "\"";
	}

	std::string AppSettings::serialize() const
	{
		std::ostringstream out;
		out << "// ModelTool settings\n";

		out << "basePaths {\n";
		for (const auto &path : basePaths)
		{
			out << "\t" << quote(path) << "\n";
		}
		out << "}\n";

		out << "recentFiles {\n";
		for (const auto &path : recentFiles)
		{
			out << "\t" << quote(path) << "\n";
		}
		out << "}\n";

		out << "window " << windowPosition.x << " " << windowPosition.y << " " << windowSize.x << " " << windowSize.y << " "
			<< (windowMaximized ? 1 : 0) << "\n";
		out << "vsync " << (vsync ? 1 : 0) << "\n";
		out << "showLog " << (showLog ? 1 : 0) << "\n";

		out << "values {\n";
		for (const auto &[key, value] : values)
		{
			out << "\t" << quote(key) << " " << quote(value) << "\n";
		}
		out << "}\n";
		return out.str();
	}

	AppSettings AppSettings::parse(const std::string &text)
	{
		AppSettings settings;
		ScriptStream stream(text);

		auto readList = [&](std::vector<std::string> &list) {
			if (stream.token() != "{")
			{
				throw mt_ex("Expected '{' in the settings");
			}
			list.clear();
			while (stream.hasNext() && stream.peekNext() != "}")
			{
				list.push_back(stream.token());
			}
			stream.token();
		};

		while (stream.hasNext())
		{
			std::string key = stream.token();
			if (key == "basePaths")
			{
				readList(settings.basePaths);
			}
			else if (key == "recentFiles")
			{
				readList(settings.recentFiles);
				if (settings.recentFiles.size() > MAX_RECENT_FILES)
				{
					settings.recentFiles.resize(MAX_RECENT_FILES);
				}
			}
			else if (key == "window")
			{
				settings.windowPosition.x = stream.parseInt(false);
				settings.windowPosition.y = stream.parseInt(false);
				settings.windowSize.x = std::max(stream.parseInt(false), 320);
				settings.windowSize.y = std::max(stream.parseInt(false), 240);
				settings.windowMaximized = stream.parseInt(false) != 0;
			}
			else if (key == "vsync")
			{
				settings.vsync = stream.parseInt(false) != 0;
			}
			else if (key == "showLog")
			{
				settings.showLog = stream.parseInt(false) != 0;
			}
			else if (key == "values")
			{
				if (stream.token() != "{")
				{
					throw mt_ex("Expected '{' in the settings");
				}
				while (stream.hasNext() && stream.peekNext() != "}")
				{
					std::string name = stream.token();
					settings.values[name] = stream.token(false);
				}
				stream.token();
			}
			else
			{
				// Settings from a newer version: skip the line
				stream.skipLine();
			}
		}

		return settings;
	}

	AppSettings AppSettings::load(const std::string &path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			return {};
		}

		std::stringstream text;
		text << in.rdbuf();
		try
		{
			return parse(text.str());
		}
		catch (const std::exception &)
		{
			return {};
		}
	}

	bool AppSettings::save(const std::string &path) const
	{
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			return false;
		}
		out << serialize();
		return out.good();
	}

	bool AppSettings::getBool(const std::string &key, bool fallback) const
	{
		auto found = values.find(key);
		return found == values.end() ? fallback : found->second == "1" || found->second == "true";
	}

	int AppSettings::getInt(const std::string &key, int fallback) const
	{
		auto found = values.find(key);
		if (found == values.end())
		{
			return fallback;
		}
		try
		{
			return std::stoi(found->second);
		}
		catch (const std::exception &)
		{
			return fallback;
		}
	}

	float AppSettings::getFloat(const std::string &key, float fallback) const
	{
		auto found = values.find(key);
		if (found == values.end())
		{
			return fallback;
		}
		try
		{
			return std::stof(found->second);
		}
		catch (const std::exception &)
		{
			return fallback;
		}
	}

	void AppSettings::set(const std::string &key, bool value)
	{
		values[key] = value ? "1" : "0";
	}

	void AppSettings::set(const std::string &key, int value)
	{
		values[key] = std::to_string(value);
	}

	void AppSettings::set(const std::string &key, float value)
	{
		std::ostringstream out;
		out << std::setprecision(6) << value;
		values[key] = out.str();
	}

	std::string settingsFolder()
	{
		namespace fs = std::filesystem;
#if defined(_WIN32)
		const char *appData = std::getenv("APPDATA");
		return appData ? (fs::path(appData) / "ModelTool").string() : "";
#elif defined(__APPLE__)
		const char *home = std::getenv("HOME");
		return home ? (fs::path(home) / "Library" / "Application Support" / "ModelTool").string() : "";
#else
		const char *config = std::getenv("XDG_CONFIG_HOME");
		if (config && config[0])
		{
			return (fs::path(config) / "ModelTool").string();
		}
		const char *home = std::getenv("HOME");
		return home ? (fs::path(home) / ".config" / "ModelTool").string() : "";
#endif
	}
}
