#include "gtest/gtest.h"
#include "settings.h"

#include <filesystem>

using namespace mt;

TEST(SettingsTest, roundTrip)
{
	AppSettings settings;
	settings.basePaths = {"/games/et/etmain", "C:\\Games\\ET\\etmain"};
	settings.addRecentFile("models/players/temperate/body.mdm");
	settings.addRecentFile("/home/user/model with spaces.md3");
	settings.windowPosition = {100, 50};
	settings.windowSize = {1600, 900};
	settings.windowMaximized = true;
	settings.vsync = false;
	settings.showLog = false;
	settings.set("view.wireframe", true);
	settings.set("view.picmip", 2);
	settings.set("camera.fov", 75.5f);

	AppSettings loaded = AppSettings::parse(settings.serialize());
	ASSERT_EQ(loaded.basePaths, settings.basePaths);
	ASSERT_EQ(loaded.recentFiles, settings.recentFiles);
	ASSERT_EQ(loaded.recentFiles[0], "/home/user/model with spaces.md3");
	ASSERT_EQ(loaded.windowPosition, settings.windowPosition);
	ASSERT_EQ(loaded.windowSize, settings.windowSize);
	ASSERT_TRUE(loaded.windowMaximized);
	ASSERT_FALSE(loaded.vsync);
	ASSERT_FALSE(loaded.showLog);
	ASSERT_TRUE(loaded.getBool("view.wireframe", false));
	ASSERT_EQ(loaded.getInt("view.picmip", 0), 2);
	ASSERT_FLOAT_EQ(loaded.getFloat("camera.fov", 90.0f), 75.5f);

	// Defaults for missing or broken values
	ASSERT_EQ(loaded.getInt("missing", 7), 7);
	loaded.values["broken"] = "abc";
	ASSERT_EQ(loaded.getInt("broken", 3), 3);
}

TEST(SettingsTest, recentFiles)
{
	AppSettings settings;
	for (int i = 0; i < 15; i++)
	{
		settings.addRecentFile("file" + std::to_string(i));
	}
	ASSERT_EQ(settings.recentFiles.size(), AppSettings::MAX_RECENT_FILES);
	ASSERT_EQ(settings.recentFiles[0], "file14");

	// An opened file moves to the top without duplicates
	settings.addRecentFile("file10");
	ASSERT_EQ(settings.recentFiles[0], "file10");
	ASSERT_EQ(std::count(settings.recentFiles.begin(), settings.recentFiles.end(), "file10"), 1);
}

TEST(SettingsTest, unknownAndBrokenInput)
{
	// Lines from newer versions are skipped
	AppSettings settings = AppSettings::parse("newSetting 1 2 3\nvsync 0\n");
	ASSERT_FALSE(settings.vsync);

	ASSERT_THROW(AppSettings::parse("basePaths \"x\""), mt_exception);

	// Values that can not be stored
	AppSettings bad;
	bad.basePaths = {"has \"quotes\""};
	ASSERT_THROW(bad.serialize(), mt_exception);

	// A missing file gives the defaults
	AppSettings missing = AppSettings::load("tmp/does-not-exist/settings.cfg");
	ASSERT_TRUE(missing.basePaths.empty());
	ASSERT_TRUE(missing.vsync);
}

TEST(SettingsTest, saveAndLoad)
{
	std::filesystem::path folder = std::filesystem::path("tmp") / "settings-test";
	std::filesystem::remove_all(folder);

	AppSettings settings;
	settings.basePaths = {"data"};
	settings.addRecentFile("models/md3/pf.md3");
	ASSERT_TRUE(settings.save((folder / "nested" / "settings.cfg").string()));

	AppSettings loaded = AppSettings::load((folder / "nested" / "settings.cfg").string());
	ASSERT_EQ(loaded.recentFiles, settings.recentFiles);

	std::filesystem::remove_all(folder);
	ASSERT_FALSE(settingsFolder().empty());
}
