#include "app_settings.h"
#include "state.h"
#include "camera.h"

namespace mt {

	namespace {
		/*! Boolean view options stored by name */
		std::vector<std::pair<const char *, bool ViewSettings::*>> viewToggles()
		{
			return {
				{"view.wireframe", &ViewSettings::wireframe},
				{"view.textures", &ViewSettings::textures},
				{"view.alpha", &ViewSettings::alpha},
				{"view.shaders", &ViewSettings::shaders},
				{"view.grid", &ViewSettings::drawGrid},
				{"view.bounds", &ViewSettings::drawBounds},
				{"view.hitboxes", &ViewSettings::drawHitboxes},
				{"view.tags", &ViewSettings::drawTags},
				{"view.tagNames", &ViewSettings::drawTagNames},
				{"view.skeleton", &ViewSettings::drawSkeleton},
				{"view.normals", &ViewSettings::drawNormals},
				{"view.dynamicLod", &ViewSettings::dynamicLod},
				{"view.gizmo", &ViewSettings::drawGizmo},
				{"view.twoSided", &ViewSettings::twoSided},
				{"view.flatShading", &ViewSettings::flatShading},
			};
		}
	}

	void applySettings(State &state, Camera &camera)
	{
		const AppSettings &settings = state.settings;
		auto &view = state.view;

		for (const auto &[key, member] : viewToggles())
		{
			view.*member = settings.getBool(key, view.*member);
		}
		view.normalLength = settings.getFloat("view.normalLength", view.normalLength);
		view.gamma = std::clamp(settings.getFloat("view.gamma", view.gamma), 0.5f, 3.0f);
		view.intensity = std::clamp(settings.getFloat("view.intensity", view.intensity), 1.0f, 4.0f);
		view.lodRatio = std::clamp(settings.getFloat("view.lodRatio", view.lodRatio), 0.0f, 1.0f);
		view.picmip = std::clamp(settings.getInt("view.picmip", view.picmip), 0, 3);
		view.textureFilter = TextureFilter(std::clamp(settings.getInt("view.textureFilter", int(view.textureFilter)), 0, 2));

		state.playback.speed = std::clamp(settings.getFloat("playback.speed", state.playback.speed), 0.0f, 4.0f);
		state.playback.interpolate = settings.getBool("playback.interpolate", state.playback.interpolate);

		camera.fov = std::clamp(settings.getFloat("camera.fov", camera.fov), 30.0f, 120.0f);

		const ImVec4 &color = state.clearColor.Value;
		state.clearColor = ImColor(settings.getFloat("background.r", color.x), settings.getFloat("background.g", color.y),
								   settings.getFloat("background.b", color.z));

		state.background = Background(std::clamp(settings.getInt("background.mode", int(state.background)), 0, 2));
		const ImVec4 &bottom = state.backgroundBottom.Value;
		state.backgroundBottom
			= ImColor(settings.getFloat("background.bottom.r", bottom.x), settings.getFloat("background.bottom.g", bottom.y),
					  settings.getFloat("background.bottom.b", bottom.z));
		if (auto sky = settings.values.find("background.sky"); sky != settings.values.end())
		{
			state.skyBox = sky->second;
		}

		state.theme = ui::Theme(std::clamp(settings.getInt("ui.theme", int(state.theme)), 0, ui::THEME_COUNT - 1));
		state.fontSize = std::clamp(settings.getFloat("ui.fontSize", state.fontSize), ui::MIN_FONT_SIZE, ui::MAX_FONT_SIZE);

		state.filesystem.setCaseInsensitive(settings.getBool("files.caseInsensitive", true));

		state.showLog = settings.showLog;
	}

	void collectSettings(State &state, const Camera &camera)
	{
		AppSettings &settings = state.settings;
		const auto &view = state.view;

		for (const auto &[key, member] : viewToggles())
		{
			settings.set(key, view.*member);
		}
		settings.set("view.normalLength", view.normalLength);
		settings.set("view.gamma", view.gamma);
		settings.set("view.intensity", view.intensity);
		settings.set("view.lodRatio", view.lodRatio);
		settings.set("view.picmip", view.picmip);
		settings.set("view.textureFilter", int(view.textureFilter));
		settings.set("playback.speed", state.playback.speed);
		settings.set("playback.interpolate", state.playback.interpolate);
		settings.set("camera.fov", camera.fov);
		settings.set("background.r", state.clearColor.Value.x);
		settings.set("background.g", state.clearColor.Value.y);
		settings.set("background.b", state.clearColor.Value.z);
		settings.set("background.mode", int(state.background));
		settings.set("background.bottom.r", state.backgroundBottom.Value.x);
		settings.set("background.bottom.g", state.backgroundBottom.Value.y);
		settings.set("background.bottom.b", state.backgroundBottom.Value.z);
		settings.values["background.sky"] = state.skyBox;

		settings.set("ui.theme", int(state.theme));
		settings.set("ui.fontSize", state.fontSize);

		settings.set("files.caseInsensitive", state.filesystem.isCaseInsensitive());

		settings.showLog = state.showLog;
		settings.basePaths = state.basePaths;
	}
}
