#include "app_settings.h"
#include "state.h"
#include "camera.h"

namespace mt {

	namespace {
		/*! Boolean view options stored by name */
		std::vector<std::pair<const char *, bool ViewSettings::*>> viewToggles()
		{
			return {
				{"view.wireframe", &ViewSettings::wireframe},	{"view.textures", &ViewSettings::textures},
				{"view.alpha", &ViewSettings::alpha},			{"view.grid", &ViewSettings::drawGrid},
				{"view.bounds", &ViewSettings::drawBounds},		{"view.hitboxes", &ViewSettings::drawHitboxes},
				{"view.tags", &ViewSettings::drawTags},			{"view.tagNames", &ViewSettings::drawTagNames},
				{"view.skeleton", &ViewSettings::drawSkeleton}, {"view.normals", &ViewSettings::drawNormals},
				{"view.dynamicLod", &ViewSettings::dynamicLod},
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
		view.lodRatio = std::clamp(settings.getFloat("view.lodRatio", view.lodRatio), 0.0f, 1.0f);
		view.picmip = std::clamp(settings.getInt("view.picmip", view.picmip), 0, 3);
		view.textureFilter = TextureFilter(std::clamp(settings.getInt("view.textureFilter", int(view.textureFilter)), 0, 2));

		state.playback.speed = std::clamp(settings.getFloat("playback.speed", state.playback.speed), 0.0f, 4.0f);
		state.playback.interpolate = settings.getBool("playback.interpolate", state.playback.interpolate);

		camera.fov = std::clamp(settings.getFloat("camera.fov", camera.fov), 30.0f, 120.0f);

		const ImVec4 &color = state.clearColor.Value;
		state.clearColor = ImColor(settings.getFloat("background.r", color.x), settings.getFloat("background.g", color.y),
								   settings.getFloat("background.b", color.z));

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
		settings.set("view.lodRatio", view.lodRatio);
		settings.set("view.picmip", view.picmip);
		settings.set("view.textureFilter", int(view.textureFilter));
		settings.set("playback.speed", state.playback.speed);
		settings.set("playback.interpolate", state.playback.interpolate);
		settings.set("camera.fov", camera.fov);
		settings.set("background.r", state.clearColor.Value.x);
		settings.set("background.g", state.clearColor.Value.y);
		settings.set("background.b", state.clearColor.Value.z);

		settings.showLog = state.showLog;
		settings.basePaths = state.basePaths;
	}
}
