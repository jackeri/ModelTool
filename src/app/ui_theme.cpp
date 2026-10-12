#include "ui_theme.h"
#include "embedded_files.h"
#include "icons_font_awesome.h"

namespace mt::ui {

	namespace {
		/**
		 * Darker variant of the ImGui dark style with a blue accent
		 */
		void darkerColors(ImGuiStyle &style)
		{
			ImGui::StyleColorsDark(&style);

			const ImVec4 accent{0.30f, 0.52f, 0.86f, 1.00f};
			auto withAlpha = [](ImVec4 color, float alpha) {
				color.w = alpha;
				return color;
			};

			ImVec4 *colors = style.Colors;
			colors[ImGuiCol_Text] = {0.88f, 0.89f, 0.91f, 1.00f};
			colors[ImGuiCol_TextDisabled] = {0.50f, 0.51f, 0.54f, 1.00f};
			colors[ImGuiCol_WindowBg] = {0.10f, 0.10f, 0.11f, 1.00f};
			colors[ImGuiCol_ChildBg] = {0.00f, 0.00f, 0.00f, 0.00f};
			colors[ImGuiCol_PopupBg] = {0.08f, 0.08f, 0.09f, 0.98f};
			colors[ImGuiCol_Border] = {0.22f, 0.22f, 0.25f, 0.60f};
			colors[ImGuiCol_FrameBg] = {0.16f, 0.16f, 0.18f, 1.00f};
			colors[ImGuiCol_FrameBgHovered] = {0.21f, 0.22f, 0.25f, 1.00f};
			colors[ImGuiCol_FrameBgActive] = {0.25f, 0.27f, 0.31f, 1.00f};
			colors[ImGuiCol_TitleBg] = {0.07f, 0.07f, 0.08f, 1.00f};
			colors[ImGuiCol_TitleBgActive] = {0.10f, 0.11f, 0.13f, 1.00f};
			colors[ImGuiCol_TitleBgCollapsed] = {0.07f, 0.07f, 0.08f, 0.75f};
			colors[ImGuiCol_MenuBarBg] = {0.08f, 0.08f, 0.09f, 1.00f};
			colors[ImGuiCol_ScrollbarBg] = {0.08f, 0.08f, 0.09f, 0.60f};
			colors[ImGuiCol_ScrollbarGrab] = {0.24f, 0.25f, 0.28f, 1.00f};
			colors[ImGuiCol_ScrollbarGrabHovered] = {0.30f, 0.31f, 0.35f, 1.00f};
			colors[ImGuiCol_ScrollbarGrabActive] = {0.36f, 0.38f, 0.42f, 1.00f};
			colors[ImGuiCol_CheckMark] = {0.45f, 0.66f, 0.98f, 1.00f};
			colors[ImGuiCol_SliderGrab] = withAlpha(accent, 0.85f);
			colors[ImGuiCol_SliderGrabActive] = {0.45f, 0.66f, 0.98f, 1.00f};
			colors[ImGuiCol_Button] = {0.19f, 0.21f, 0.25f, 1.00f};
			colors[ImGuiCol_ButtonHovered] = {0.26f, 0.30f, 0.38f, 1.00f};
			colors[ImGuiCol_ButtonActive] = withAlpha(accent, 1.00f);
			colors[ImGuiCol_Header] = {0.18f, 0.20f, 0.24f, 1.00f};
			colors[ImGuiCol_HeaderHovered] = {0.24f, 0.28f, 0.36f, 1.00f};
			colors[ImGuiCol_HeaderActive] = {0.27f, 0.34f, 0.46f, 1.00f};
			colors[ImGuiCol_Separator] = {0.20f, 0.20f, 0.23f, 1.00f};
			colors[ImGuiCol_SeparatorHovered] = withAlpha(accent, 0.75f);
			colors[ImGuiCol_SeparatorActive] = withAlpha(accent, 1.00f);
			colors[ImGuiCol_ResizeGrip] = withAlpha(accent, 0.15f);
			colors[ImGuiCol_ResizeGripHovered] = withAlpha(accent, 0.60f);
			colors[ImGuiCol_ResizeGripActive] = withAlpha(accent, 0.90f);
			colors[ImGuiCol_Tab] = {0.12f, 0.12f, 0.14f, 1.00f};
			colors[ImGuiCol_TabHovered] = {0.24f, 0.28f, 0.36f, 1.00f};
			colors[ImGuiCol_TabSelected] = {0.17f, 0.19f, 0.23f, 1.00f};
			colors[ImGuiCol_TabSelectedOverline] = accent;
			colors[ImGuiCol_TabDimmed] = {0.09f, 0.09f, 0.10f, 1.00f};
			colors[ImGuiCol_TabDimmedSelected] = {0.13f, 0.14f, 0.16f, 1.00f};
			colors[ImGuiCol_DockingPreview] = withAlpha(accent, 0.50f);
			colors[ImGuiCol_DockingEmptyBg] = {0.06f, 0.06f, 0.07f, 1.00f};
			colors[ImGuiCol_TextSelectedBg] = withAlpha(accent, 0.35f);
			colors[ImGuiCol_NavHighlight] = accent;
			colors[ImGuiCol_ModalWindowDimBg] = {0.00f, 0.00f, 0.00f, 0.55f};
		}
	}

	const char *themeName(Theme theme)
	{
		switch (theme)
		{
		case Theme::Dark: return "Dark";
		case Theme::Light: return "Light";
		case Theme::Darker:
		default: return "Darker";
		}
	}

	void applyTheme(Theme theme)
	{
		ImGuiStyle &style = ImGui::GetStyle();
		style = ImGuiStyle();

		switch (theme)
		{
		case Theme::Dark: ImGui::StyleColorsDark(&style); break;
		case Theme::Light: ImGui::StyleColorsLight(&style); break;
		case Theme::Darker:
		default: darkerColors(style); break;
		}

		style.WindowRounding = 4.0f;
		style.ChildRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.ScrollbarRounding = 6.0f;
		style.FramePadding = {6.0f, 4.0f};
		style.ItemSpacing = {8.0f, 5.0f};
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.SeparatorTextBorderSize = 1.0f;
	}

	glm::vec3 sceneBackground(Theme theme)
	{
		switch (theme)
		{
		case Theme::Dark: return {0.22f, 0.24f, 0.27f};
		case Theme::Light: return glm::vec3{114, 144, 154} / 255.0f;
		case Theme::Darker:
		default: return {0.15f, 0.16f, 0.18f};
		}
	}

	bool loadFonts(float size, float pixelScale)
	{
		ImGuiIO &io = ImGui::GetIO();
		io.Fonts->Clear();
		pixelScale = std::max(pixelScale, 1.0f);
		io.FontGlobalScale = 1.0f / pixelScale;

		const EmbeddedFile text = embeddedFile("Hack-Regular.ttf");
		const EmbeddedFile icons = embeddedFile("fontawesome-webfont.ttf");
		if (!text.data)
		{
			ImFontConfig config;
			config.SizePixels = size * pixelScale;
			io.Fonts->AddFontDefault(&config);
			return false;
		}

		// The atlas does not own the data, it is part of the program
		ImFontConfig textConfig;
		textConfig.FontDataOwnedByAtlas = false;
		io.Fonts->AddFontFromMemoryTTF((void *)text.data, int(text.size), size * pixelScale, &textConfig);

		if (icons.data)
		{
			// Icons merged into the text font, a bit larger like in the old tool (14 / 16)
			static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
			const float iconSize = size * (16.0f / 14.0f) * pixelScale;
			ImFontConfig iconConfig;
			iconConfig.FontDataOwnedByAtlas = false;
			iconConfig.MergeMode = true;
			iconConfig.PixelSnapH = true;
			iconConfig.GlyphMinAdvanceX = iconSize;
			iconConfig.GlyphOffset = {0, pixelScale};
			io.Fonts->AddFontFromMemoryTTF((void *)icons.data, int(icons.size), iconSize, &iconConfig, iconRanges);
		}

		return io.Fonts->Build();
	}
}
