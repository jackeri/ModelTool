#pragma once

#include "mt.h"

namespace mt::ui {

	/*! Color scheme of the user interface */
	enum class Theme
	{
		Darker,
		Dark,
		Light
	};

	constexpr int THEME_COUNT = 3;
	constexpr float DEFAULT_FONT_SIZE = 14.0f; ///< the old LegacyModelView font size
	constexpr float MIN_FONT_SIZE = 10.0f;
	constexpr float MAX_FONT_SIZE = 24.0f;

	/**
	 * @return display name of a theme
	 */
	const char *themeName(Theme theme);

	/**
	 * Set the ImGui style (colors, rounding, spacing) of a theme
	 * @param theme theme to use
	 */
	void applyTheme(Theme theme);

	/**
	 * Scene background that goes with a theme
	 * @param theme theme
	 * @return RGB color
	 */
	glm::vec3 sceneBackground(Theme theme);

	/**
	 * Rebuild the font atlas: Hack (the old tool's font) with the Font Awesome icons merged in, both built into the
	 * program. The fonts are rasterized at the screen's pixel density and scaled back so the text stays sharp on HiDPI
	 * screens. The renderer's font texture has to be recreated after this.
	 * @param size font size in window units
	 * @param pixelScale framebuffer pixels per window unit
	 * @return false if the built-in fonts are missing (the ImGui default font is used then)
	 */
	bool loadFonts(float size, float pixelScale);
}
