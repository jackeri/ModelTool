#include "app_options.h"

namespace mt {

	AppOptions AppOptions::parse(int argc, char *argv[])
	{
		AppOptions options;

		for (int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];
			auto value = [&]() -> std::string {
				if (i + 1 >= argc)
				{
					throw mt_ex("Missing value for " + arg);
				}
				return argv[++i];
			};

			if (arg == "--base")
			{
				options.basePaths.push_back(value());
			}
			else if (arg == "--model")
			{
				options.model = value();
			}
			else if (arg == "--animation")
			{
				options.animations.push_back(value());
			}
			else if (arg == "--frame")
			{
				options.frame = std::stoi(value());
			}
			else if (arg == "--screenshot")
			{
				options.screenshot = value();
			}
			else if (arg == "--export")
			{
				options.exports.push_back(value());
			}
			else if (arg == "--frames")
			{
				options.frames = std::stoi(value());
			}
			else if (arg == "--ui")
			{
				options.screenshotUi = true;
			}
			else if (arg == "--size")
			{
				std::string size = value();
				auto x = size.find('x');
				if (x == std::string::npos)
				{
					throw mt_ex("Invalid size, expected WxH: " + size);
				}
				options.width = std::stoi(size.substr(0, x));
				options.height = std::stoi(size.substr(x + 1));
			}
			else if (arg == "--view")
			{
				std::stringstream list(value());
				for (std::string item; std::getline(list, item, ',');)
				{
					options.view.push_back(item);
				}
			}
			else
			{
				throw mt_ex("Unknown argument: " + arg + "\n" + usage());
			}
		}

		return options;
	}

	std::string AppOptions::usage()
	{
		return "Usage: ModelTool [--base folder]... [--model path] [--animation path]... [--frame n]\n"
			   "                 [--view wireframe,notextures,alpha,nogrid,bounds,nohitboxes,tags,skeleton,normals,dynamiclod]\n"
			   "                 [--export out.ext]... [--screenshot out.png [--ui]] [--size WxH] [--frames n]";
	}
}
