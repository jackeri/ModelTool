#include "gtest/gtest.h"
#include "app_settings.h"
#include "camera.h"
#include "state.h"

using namespace mt;

TEST(AppSettingsTest, ViewAndBackgroundRoundTrip)
{
	auto &state = singleton<State>();
	Camera camera;
	const ViewSettings saved = state.view;

	state.view.twoSided = true;
	state.view.flatShading = true;
	state.view.gamma = 1.3f;
	state.view.intensity = 2.0f;
	state.background = Background::SkyBox;
	state.backgroundBottom = ImColor(0.1f, 0.2f, 0.3f);
	state.skyBox = "textures/skies_sd/wurzburg_env/sky";
	collectSettings(state, camera);

	// Through the settings text, like a restart
	state.settings = AppSettings::parse(state.settings.serialize());
	state.view = ViewSettings{};
	state.background = Background::Color;
	state.backgroundBottom = ImColor(1.0f, 1.0f, 1.0f);
	state.skyBox.clear();
	applySettings(state, camera);

	EXPECT_TRUE(state.view.twoSided);
	EXPECT_TRUE(state.view.flatShading);
	EXPECT_FLOAT_EQ(state.view.gamma, 1.3f);
	EXPECT_FLOAT_EQ(state.view.intensity, 2.0f);
	EXPECT_EQ(state.background, Background::SkyBox);
	EXPECT_FLOAT_EQ(state.backgroundBottom.Value.z, 0.3f);
	EXPECT_EQ(state.skyBox, "textures/skies_sd/wurzburg_env/sky");

	// Out of range values are clamped
	state.settings.set("view.gamma", 10.0f);
	state.settings.set("background.mode", 7);
	applySettings(state, camera);
	EXPECT_FLOAT_EQ(state.view.gamma, 3.0f);
	EXPECT_EQ(state.background, Background::SkyBox);

	state.view = saved;
	state.background = Background::Color;
	state.skyBox.clear();
	state.settings = AppSettings{};
}
