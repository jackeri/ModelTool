#include "imgui_view.h"
#include "model_ms3d.h"
#include <ImGuizmo.h>
#include "icons_font_awesome.h"
#include "gl_window.h"
#include "state.h"
#include "hitbox.h"
#include "scene_panel.h"
#include "texture.h"
#include "normals.h"
#include "lod.h"
#include "skeletal_tools.h"
#include "et_armature.h"
#include "model_export.h"
#include "model_mdm.h"
#include "model_mdx.h"
#include "model_md3.h"
#include "model_md5.h"
#include "model_scripts.h"
#include "model_stl.h"
#include "settings.h"

#include <filesystem>
#include "model_scripts.h"
#include "skeletal_model.h"

// Dockspace internal api is needed
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"

#include "backends/imgui_impl_opengl3.h"

namespace mt {

	const char *LOAD_MODEL = "Load model";
	const char *LOAD_ANIMATION = "Load animation";

	ImGuiView::ImGuiView()
	{
		logger = spdlog::get(MT_LOGGER);
	}

	bool ImGuiView::setup(GLWindow *window, const char *glslVersion)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking

		// The layout is kept with the settings, screenshots do not read or write it
		glfwWindow = window->system();
		if (singleton<State>().persistSettings && !settingsFolder().empty())
		{
			iniPath = settingsFolder() + "/imgui.ini";
			io.IniFilename = iniPath.c_str();
		}
		else
		{
			io.IniFilename = nullptr;
		}
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

		// The style and the fonts are set up in updateStyle() before the first frame

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window->system(), true);

		ImGui_ImplOpenGL3_Init(glslVersion);
		return true;
	}

	void ImGuiView::updateStyle()
	{
		auto &state = singleton<State>();

		if (int(state.theme) != appliedTheme)
		{
			ui::applyTheme(state.theme);
			appliedTheme = int(state.theme);
		}

		// Fonts are rasterized at the framebuffer's pixel density (2 on Retina screens) so the text is sharp
		int width = 0, height = 0, fbWidth = 0, fbHeight = 0;
		glfwGetWindowSize(glfwWindow, &width, &height);
		glfwGetFramebufferSize(glfwWindow, &fbWidth, &fbHeight);
		const float pixelScale = width > 0 && fbWidth > 0 ? float(fbWidth) / float(width) : appliedPixelScale;

		if (state.fontSize != appliedFontSize || (pixelScale > 0 && pixelScale != appliedPixelScale))
		{
			appliedFontSize = state.fontSize;
			appliedPixelScale = std::max(pixelScale, 1.0f);
			if (!ui::loadFonts(appliedFontSize, appliedPixelScale))
			{
				logger->warn("The built-in fonts are missing, using the ImGui default font");
			}
			ImGui_ImplOpenGL3_DestroyFontsTexture();
		}
	}

	void ImGuiView::startFrame()
	{
		updateStyle();
		ImGui_ImplOpenGL3_NewFrame();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiView::endFrame()
	{
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow *backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void ImGuiView::draw()
	{
		auto &state = singleton<State>();

		// Enable the whole viewport docking
		ImGuiID dockId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);

		// DockSpaceOverViewport creates the node, so build the default layout while nothing is docked in it yet
		ImGuiDockNode *dockNode = ImGui::DockBuilderGetNode(dockId);
		if (dockNode == nullptr || (dockNode->IsLeafNode() && dockNode->Windows.Size == 0))
		{
			ImGui::DockBuilderRemoveNode(dockId);							 // Clear out existing layout
			ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace); // Add empty node
			ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);

			// Properties on the left, the scene takes the rest
			ImGuiID sceneId = dockId;
			ImGuiID leftId = ImGui::DockBuilderSplitNode(sceneId, ImGuiDir_Left, 0.25f, nullptr, &sceneId);

			ImGuiID logId = ImGui::DockBuilderSplitNode(sceneId, ImGuiDir_Down, 0.22f, nullptr, &sceneId);

			ImGui::DockBuilderDockWindow("Properties", leftId);
			ImGui::DockBuilderDockWindow("Scene", sceneId);
			ImGui::DockBuilderDockWindow("Log", logId);
			ImGui::DockBuilderFinish(dockId);
		}

		drawMenu();
		handleAttachRequest();
		drawScaleBoundsWindow();
		drawSettingsWindow();
		drawExportAllWindow();

		// We might need to execute some ImGui code out of menus or a window for example, this is here for that
		if (!m_lateExecution.empty())
		{
			for (auto &exec : m_lateExecution)
			{
				exec();
			}
			m_lateExecution.clear();
		}

		// Ask for the base folder the first time
		if (!state.filesystem.hasSources() && !browser.isOpen() && !askedBaseFolder)
		{
			askedBaseFolder = true;
			showBaseFolderBrowser(false);
		}

		browser.handle();
		baseBrowser.render();
		logPanel.render(state.showLog);
		handleShortcuts();
	}

	void ImGuiView::shutdown()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiView::drawMenu()
	{
		auto &state = singleton<State>();

		// Setup the main menu bar.
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open model...", shortcutLabel("Ctrl+O").c_str()))
				{
					openModelFromDisk();
				}

				if (ImGui::MenuItem(ICON_FA_ARCHIVE " Open from base folders..."))
				{
					baseBrowser.show("Open from base folders", openExtensions(), [this](const std::string &path) { openModel(path); });
				}

				if (ImGui::BeginMenu(ICON_FA_HISTORY " Recent models", !state.settings.recentFiles.empty()))
				{
					std::string selected;
					for (const auto &path : state.settings.recentFiles)
					{
						if (ImGui::MenuItem(path.c_str()))
						{
							selected = path;
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Clear"))
					{
						state.settings.recentFiles.clear();
					}
					ImGui::EndMenu();

					if (!selected.empty())
					{
						openModel(selected);
					}
				}

				if (ImGui::BeginMenu(ICON_FA_DATABASE " Base folders"))
				{
					if (state.basePaths.empty())
					{
						ImGui::TextDisabled("No base folders");
					}
					for (const auto &path : state.basePaths)
					{
						ImGui::TextDisabled("%s", path.c_str());
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Add base folder..."))
					{
						showBaseFolderBrowser(true);
					}
					if (ImGui::MenuItem("Change base folder..."))
					{
						showBaseFolderBrowser(false);
					}
					if (ImGui::MenuItem("Clear base folders", nullptr, false, !state.basePaths.empty()))
					{
						state.clearBasePaths();
						logger->info("Cleared the base folders");
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem(ICON_FA_FILM " Load animation from base folders...", nullptr, false, !!state.model))
				{
					baseBrowser.show("Load animation", model::io::animationExtensions(), [this](const std::string &path) {
						try
						{
							singleton<State>().loadAnimation(path);
							logger->info("Loaded animation {}", path);
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load animation: {}", ex.what());
						}
					});
				}

				if (ImGui::MenuItem(ICON_FA_FILM " Load animation...", nullptr, false, !!state.model))
				{
					browser.show("Load animation", [this](const std::string &animation) {
						rememberFolder(animation);
						try
						{
							singleton<State>().loadAnimation(animation);
							logger->info("Loaded animation {}", animation);
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load animation: {}", ex.what());
						}
					});
					browser.setTypeFilters(model::io::animationExtensions());
					browser.setDirectory(startFolder());
				}

				if (ImGui::MenuItem(ICON_FA_CUBE " Load hitboxes", nullptr, false, (!!state.model && state.model->skeletalModel())))
				{
					browser.show("Load hitboxes", [&](const std::string &file) {
						logger->info("Selected filename: {}", file);
						try
						{
							model::Hitbox::loadHitboxes(state.filesystem.loadFile(file), *state.model->getHitboxes());
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load hitboxes: {}", ex.what());
						}
					});
					browser.setTypeFilters({".hitboxes", ".htx"});
				}

				auto *skeletal = dynamic_cast<model::skeletal::SkeletalModel *>(state.model.get());
				if (ImGui::MenuItem(ICON_FA_TAGS " Load tags", nullptr, false, skeletal && !skeletal->joints.empty()))
				{
					browser.show("Load tags", [&](const std::string &file) {
						logger->info("Selected filename: {}", file);
						try
						{
							auto *model = dynamic_cast<model::skeletal::SkeletalModel *>(state.model.get());
							if (model)
							{
								model::loadTags(*model, state.filesystem.loadFile(file));
							}
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load tags: {}", ex.what());
						}
					});
					browser.setTypeFilters({".tag"});
				}

				if (ImGui::MenuItem(ICON_FA_FLOPPY_O " Save hitboxes", nullptr, false,
									(!!state.model && state.model->skeletalModel() && !state.model->getHitboxes()->empty())))
				{
					browser.show("Save hitboxes", [&](const std::string &file) {
						logger->info("Selected filename: {}", file);
						try
						{
							model::Hitbox::saveHitboxes(file, *state.model->getHitboxes());
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to save hitboxes: {}", ex.what());
						}
					});
					browser.setTypeFilters({".hitboxes"});
				}

				ImGui::Separator();
				drawExportMenu();
				ImGui::Separator();

				// Close the current model
				if (ImGui::MenuItem(ICON_FA_TIMES " Close", nullptr, false, !!state.model))
				{
					state.closeModels();
				}

				ImGui::Separator();

				if (ImGui::MenuItem(ICON_FA_COG " Settings...", shortcutLabel("Ctrl+,").c_str()))
				{
					showSettings = true;
				}
				ImGui::Separator();

				if (ImGui::MenuItem(ICON_FA_SIGN_OUT " Quit", shortcutLabel("Ctrl+Q").c_str()))
				{
					glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
				}

				ImGui::EndMenu();
			}

			drawEditMenu();

			if (ImGui::BeginMenu("View"))
			{
				auto &view = state.view;
				ImGui::MenuItem("Wireframe", "W", &view.wireframe);
				ImGui::MenuItem("Textures", "T", &view.textures);
				ImGui::MenuItem("Alpha", nullptr, &view.alpha);
				ImGui::MenuItem("Interpolation", "N", &state.playback.interpolate);
				ImGui::MenuItem("Dynamic LOD", "L", &view.dynamicLod);
				ImGui::Separator();
				ImGui::MenuItem("Grid and origin", "Alt+O", &view.drawGrid);
				ImGui::MenuItem("Bounding box", shortcutLabel("Ctrl+B").c_str(), &view.drawBounds);
				ImGui::MenuItem("Hitboxes", nullptr, &view.drawHitboxes);
				ImGui::MenuItem("Tags", "Alt+T", &view.drawTags);
				ImGui::MenuItem("Tag names", nullptr, &view.drawTagNames, view.drawTags);
				ImGui::MenuItem("Skeleton", "Alt+S", &view.drawSkeleton);
				ImGui::MenuItem("Normals", "Alt+N", &view.drawNormals);
				ImGui::MenuItem("Orientation gizmo", nullptr, &view.drawGizmo);
				if (view.drawNormals)
				{
					ImGui::SliderFloat("Normal length", &view.normalLength, 0.005f, 0.2f, "%.3f");
				}
				ImGui::Separator();

				const char *filters[] = {"Nearest", "Linear", "Trilinear"};
				int filter = int(view.textureFilter);
				if (ImGui::Combo("Texture filter", &filter, filters, IM_ARRAYSIZE(filters)))
				{
					view.textureFilter = TextureFilter(filter);
				}
				ImGui::SliderInt("Picmip", &view.picmip, 0, 3);
				ImGui::Separator();

				if (scene)
				{
					ImGui::SliderFloat("Field of view", &scene->getCamera().fov, 30.0f, 120.0f, "%.0f");
				}
				if (ImGui::MenuItem(ICON_FA_CAMERA " Reset camera"))
				{
					state.resetCamera = true;
				}
				if (ImGui::MenuItem(ICON_FA_CROSSHAIRS " Frame model", "F", false, !!state.model))
				{
					state.frameCamera = true;
				}
				ImGui::Separator();
				ImGui::MenuItem("Log", "Shift+Esc", &state.showLog);

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void ImGuiView::drawEditMenu()
	{
		auto &state = singleton<State>();
		Model *model = state.model.get();
		auto *skeletal = dynamic_cast<model::skeletal::SkeletalModel *>(model);

		if (!ImGui::BeginMenu("Edit"))
		{
			return;
		}

		if (ImGui::MenuItem("Recalculate normals", nullptr, false, model != nullptr))
		{
			if (!model::calculateNormals(*model))
			{
				logger->warn("The model has no bind pose or frames to calculate the normals from");
			}
		}

		if (ImGui::MenuItem("Calculate LOD", nullptr, false, model != nullptr))
		{
			try
			{
				if (model::calculateLod(*model))
				{
					logger->info("Calculated the level of detail data");
				}
				else
				{
					logger->warn("The model has nothing to calculate the level of detail from");
				}
			}
			catch (const std::exception &ex)
			{
				logger->error("Failed to calculate the level of detail: {}", ex.what());
			}
		}

		if (ImGui::MenuItem("Optimize vertices", nullptr, false, skeletal != nullptr))
		{
			logger->info("Removed {} duplicate vertices", model::skeletal::optimizeVertices(*skeletal));
		}

		if (ImGui::MenuItem("Scale bounding box...", nullptr, false, model != nullptr && model->hasFrames()))
		{
			showScaleBounds = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Apply transform", nullptr, false, model != nullptr))
		{
			model::bakeTransform(*model);
			state.frameCamera = true;
			logger->info("The transform is now part of the model data");
		}

		if (ImGui::MenuItem("Bake pose", nullptr, false, skeletal && model->currentFrame >= 0 && model->hasFrames()))
		{
			model::skeletal::bakeBindPose(*skeletal, model->currentFrame);
			logger->info("Frame {} is now the bind pose", model->currentFrame);
		}

		if (ImGui::MenuItem("Bake tags", nullptr, false, skeletal != nullptr))
		{
			logger->info("Baked {} bones into tags", model::skeletal::bakeTags(*skeletal));
		}

		if (ImGui::MenuItem("Reorganize armature", nullptr, false, skeletal != nullptr))
		{
			try
			{
				logger->info("Moved {} bones to the default armature order", model::et::reorganizeArmature(*skeletal));
			}
			catch (const std::exception &ex)
			{
				logger->error("Could not reorganize the armature: {}", ex.what());
			}
		}

		if (ImGui::MenuItem("Check armature", nullptr, false, skeletal != nullptr))
		{
			auto issues = model::et::checkArmature(*skeletal);
			if (issues.empty())
			{
				logger->info("The armature matches the default ET armature");
			}
			for (const auto &issue : issues)
			{
				logger->warn("{}", issue);
			}
		}

		ImGui::EndMenu();
	}

	void ImGuiView::drawSettingsWindow()
	{
		auto &state = singleton<State>();
		const char *title = ICON_FA_COG " Settings";
		if (showSettings && !ImGui::IsPopupOpen(title))
		{
			ImGui::OpenPopup(title);
			settingsFontSize = state.fontSize;
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (!ImGui::BeginPopupModal(title, &showSettings, ImGuiWindowFlags_AlwaysAutoResize))
		{
			showSettings = false;
			return;
		}

		ImGui::SeparatorText("Interface");
		int theme = int(state.theme);
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
		if (ImGui::Combo("Theme", &theme, "Darker\0Dark\0Light\0"))
		{
			// The scene background follows the theme, it can be changed after that
			state.theme = ui::Theme(theme);
			glm::vec3 background = ui::sceneBackground(state.theme);
			state.clearColor = ImColor(background.r, background.g, background.b);
		}

		// The font atlas is rebuilt when the slider is let go
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
		ImGui::SliderFloat("Font size", &settingsFontSize, ui::MIN_FONT_SIZE, ui::MAX_FONT_SIZE, "%.0f");
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			state.fontSize = std::round(settingsFontSize);
		}

		if (ImGui::Checkbox("Vertical sync", &state.settings.vsync))
		{
			glfwSwapInterval(state.settings.vsync ? 1 : 0);
		}
		ImGui::SetItemTooltip("Limit the frame rate to the screen's refresh rate");

		ImGui::SeparatorText("Files");
		bool caseInsensitive = state.filesystem.isCaseInsensitive();
		if (ImGui::Checkbox("Ignore the case of file names", &caseInsensitive))
		{
			state.filesystem.setCaseInsensitive(caseInsensitive);
			// Textures that were not found before can be found now (and the other way around)
			state.textures.clear();
		}
		ImGui::SetItemTooltip("Find Models/Head.MD3 when models/head.md3 is asked for, like the game does");

		ImGui::SeparatorText("Scene");
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
		ImGui::ColorEdit3("Background", (float *)&state.clearColor);
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_UNDO " Theme default"))
		{
			glm::vec3 background = ui::sceneBackground(state.theme);
			state.clearColor = ImColor(background.r, background.g, background.b);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::TextDisabled("Settings are saved when the program closes");

		if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			showSettings = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void ImGuiView::drawScaleBoundsWindow()
	{
		auto &state = singleton<State>();
		if (!showScaleBounds || !state.model)
		{
			showScaleBounds = false;
			return;
		}

		static float scale = 1.0f;
		static int axes = int(model::skeletal::BoundsAxes::All);

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::Begin("Scale bounding box", &showScaleBounds, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::TextDisabled("Scales the frame bounds the game uses for culling");
			ImGui::SliderFloat("Scale", &scale, 0.1f, 5.0f);
			ImGui::Combo("Axes", &axes, "X\0Y\0Z\0X and Y\0All\0");

			if (ImGui::Button("Apply"))
			{
				model::skeletal::scaleBounds(*state.model, scale, model::skeletal::BoundsAxes(axes));
				logger->info("Scaled the bounds by {}", scale);
				showScaleBounds = false;
				scale = 1.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				showScaleBounds = false;
				scale = 1.0f;
			}
		}
		ImGui::End();
	}

	namespace {
		using ModelExport = std::function<void(const Model &, const std::string &)>;

		/*! An export format offered in the menus */
		struct ExportFormat {
			const char *label;
			const char *ext;
			ModelExport write;
			bool skeletal; ///< needs a skeletal model
			bool frames;   ///< needs frames (or a bind pose for the skeletal formats that write it as a frame)
		};

		const std::vector<ExportFormat> &exportFormats()
		{
			static const std::vector<ExportFormat> formats = {
				{"MDM (ET mesh)", ".mdm", model::writeMDM, true, false},
				{"MDX (ET animation)", ".mdx", model::writeMDX, true, false},
				{"MDS (RtCW model)", ".mds", model::writeMDS, true, false},
				{"MD5 mesh", ".md5mesh", model::writeMD5Mesh, true, false},
				{"MD5 animation", ".md5anim", model::writeMD5Anim, true, true},
				{"MD3", ".md3", model::writeMD3, false, false},
				{"MD3 tags only", ".md3", model::writeMD3Tags, false, false},
				{"MDC (compressed MD3, full frames)", ".mdc", [](const Model &m, const std::string &p) { model::writeMDC(m, p); }, false,
				 false},
				{"MS3D (MilkShape 3D)", ".ms3d", model::writeMS3D, true, false},
				{"Animation sequences (.aninc)", ".aninc", model::writeAninc, false, false},
				{"Tags file", ".tag", model::writeTags, true, false},
				{"STL (binary)", ".stl", [](const Model &m, const std::string &p) { model::writeSTL(m, p, true); }, false, false},
				{"STL (text)", ".stl", [](const Model &m, const std::string &p) { model::writeSTL(m, p, false); }, false, false},
			};
			return formats;
		}

		bool canExport(const Model &model, const ExportFormat &format)
		{
			return (!format.skeletal || model.skeletalModel()) && (!format.frames || model.hasFrames());
		}

		std::string withExtension(std::string path, const std::string &ext)
		{
			if (model::io::normalizeExtension(path) != ext)
			{
				path += ext;
			}
			return path;
		}
	}

	void ImGuiView::drawExportMenu()
	{
		auto &state = singleton<State>();
		Model *model = state.model.get();

		if (!ImGui::BeginMenu(ICON_FA_SHARE_SQUARE_O " Export", model != nullptr))
		{
			return;
		}

		for (const auto &format : exportFormats())
		{
			if (ImGui::MenuItem(format.label, nullptr, false, canExport(*model, format)))
			{
				const ExportFormat *selected = &format;
				browser.show(std::string("Export ") + format.label, [this, selected](const std::string &file) {
					auto &current = singleton<State>();
					if (!current.model)
					{
						return;
					}

					const std::string path = withExtension(file, selected->ext);
					try
					{
						selected->write(*current.model, path);
						logger->info("Exported {}", path);
					}
					catch (const std::exception &ex)
					{
						logger->error("Export to {} failed: {}", path, ex.what());
					}
				});
				browser.setTypeFilters({format.ext});
			}
		}

		ImGui::Separator();
		if (ImGui::MenuItem(ICON_FA_DOWNLOAD " Export all..."))
		{
			showExportAll = true;
			if (exportName.empty())
			{
				exportName = std::filesystem::path(state.modelPath).stem().string();
			}
		}

		ImGui::EndMenu();
	}

	void ImGuiView::drawExportAllWindow()
	{
		auto &state = singleton<State>();
		if (!showExportAll || !state.model)
		{
			showExportAll = false;
			return;
		}

		const auto &formats = exportFormats();
		if (exportSelected.size() != formats.size())
		{
			// The ET player model files are selected by default
			exportSelected.assign(formats.size(), false);
			for (size_t i = 0; i < formats.size(); i++)
			{
				std::string ext = formats[i].ext;
				exportSelected[i] = ext == ".mdm" || ext == ".mdx" || ext == ".aninc";
			}
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::Begin("Export all", &showExportAll, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::InputText("Folder", &exportFolder);
			ImGui::SameLine();
			if (ImGui::Button("Browse"))
			{
				browser.show("Export folder", [this](const std::string &folder) { exportFolder = folder; }, true);
			}
			ImGui::InputText("File name", &exportName);
			ImGui::TextDisabled("Every selected file is written as <folder>/<file name>.<extension>");
			ImGui::Separator();

			for (size_t i = 0; i < formats.size(); i++)
			{
				bool selected = exportSelected[i];
				ImGui::BeginDisabled(!canExport(*state.model, formats[i]));
				if (ImGui::Checkbox(formats[i].label, &selected))
				{
					exportSelected[i] = selected;
				}
				ImGui::EndDisabled();
			}

			ImGui::Separator();
			const bool ready = !exportFolder.empty() && !exportName.empty();
			ImGui::BeginDisabled(!ready);
			if (ImGui::Button("Export"))
			{
				int written = 0, failed = 0;
				for (size_t i = 0; i < formats.size(); i++)
				{
					if (!exportSelected[i] || !canExport(*state.model, formats[i]))
					{
						continue;
					}

					std::string name = exportName;
					if (std::string(formats[i].label) == "MD3 tags only")
					{
						name += "_tags";
					}
					else if (std::string(formats[i].label) == "STL (text)")
					{
						name += "_text";
					}

					const std::string path = (std::filesystem::path(exportFolder) / (name + formats[i].ext)).string();
					try
					{
						formats[i].write(*state.model, path);
						logger->info("Exported {}", path);
						written++;
					}
					catch (const std::exception &ex)
					{
						logger->error("Export to {} failed: {}", path, ex.what());
						failed++;
					}
				}
				logger->info("Export all: {} files written, {} failed", written, failed);
				if (failed == 0)
				{
					showExportAll = false;
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Close"))
			{
				showExportAll = false;
			}
		}
		ImGui::End();
	}

	std::string ImGuiView::shortcutLabel(const std::string &label)
	{
#ifdef __APPLE__
		// ImGui maps Cmd to its Ctrl modifier on macOS
		if (label.rfind("Ctrl+", 0) == 0)
		{
			return "Cmd+" + label.substr(5);
		}
#endif
		return label;
	}

	std::vector<std::string> ImGuiView::openExtensions()
	{
		// animation.cfg opens a player model
		auto extensions = model::io::modelExtensions();
		extensions.emplace_back(".cfg");
		return extensions;
	}

	void ImGuiView::handleAttachRequest()
	{
		auto &state = singleton<State>();
		if (!state.attachRequest)
		{
			return;
		}
		const AttachRequest request = *state.attachRequest;
		state.attachRequest.reset();

		auto attach = [this, request](const std::string &path) {
			try
			{
				singleton<State>().attachModel(path, request.parent, request.point);
				logger->info("Attached {} to {}", path, request.point);
			}
			catch (const std::exception &ex)
			{
				logger->error("Failed to attach {}: {}", path, ex.what());
			}
		};

		const std::string title = "Attach a model to " + request.point;
		if (request.fromBase)
		{
			baseBrowser.show(title, model::io::modelExtensions(), attach);
		}
		else
		{
			browser.show(title, [this, attach](const std::string &path) {
				rememberFolder(path);
				attach(path);
			});
			browser.setTypeFilters(model::io::modelExtensions());
			browser.setDirectory(startFolder());
		}
	}

	void ImGuiView::openModel(const std::string &path)
	{
		try
		{
			singleton<State>().loadModel(path);
			logger->info("Loaded model {}", path);
		}
		catch (const std::exception &ex)
		{
			logger->error("Failed to load model {}: {}", path, ex.what());
		}
	}

	// Settings key of the folder of the last model or animation opened from the disk
	static const char *LAST_FOLDER_KEY = "files.lastFolder";

	std::string ImGuiView::startFolder() const
	{
		auto &state = singleton<State>();

		// The folder of the last opened file (also from earlier sessions), or the base folder before anything was opened
		auto last = state.settings.values.find(LAST_FOLDER_KEY);
		std::error_code error;
		if (last != state.settings.values.end() && std::filesystem::is_directory(last->second, error))
		{
			return last->second;
		}

		if (!state.basePaths.empty())
		{
			const std::filesystem::path base(state.basePaths.front());
			return std::filesystem::is_directory(base, error) ? base.string() : base.parent_path().string(); // pk3 files
		}
		return "";
	}

	void ImGuiView::rememberFolder(const std::string &file)
	{
		std::error_code error;
		auto folder = std::filesystem::absolute(std::filesystem::path(file).parent_path(), error);
		if (!error)
		{
			singleton<State>().settings.values[LAST_FOLDER_KEY] = folder.lexically_normal().string();
		}
	}

	void ImGuiView::openModelFromDisk()
	{
		browser.show("Open model", [this](const std::string &path) {
			rememberFolder(path);
			openModel(path);
		});
		browser.setTypeFilters(openExtensions());
		browser.setDirectory(startFolder());
	}

	void ImGuiView::showBaseFolderBrowser(bool add)
	{
		browser.show(
			add ? "Add base folder" : "Select the base folder (the game's mod folder, like etmain)",
			[this, add](const std::string &path) {
				auto &state = singleton<State>();
				if (!add)
				{
					state.clearBasePaths();
				}
				if (state.addBasePath(path))
				{
					logger->info("Base folder: {}", path);
				}
				else
				{
					logger->error("Invalid base folder: {}", path);
				}
			},
			true);
	}

	void ImGuiView::handleShortcuts()
	{
		auto &io = ImGui::GetIO();
		if (io.WantTextInput)
		{
			return;
		}

		auto &state = singleton<State>();
		auto &view = state.view;
		auto pressed = [](ImGuiKeyChord chord) { return ImGui::IsKeyChordPressed(chord); };

		if (pressed(ImGuiMod_Ctrl | ImGuiKey_O))
		{
			openModelFromDisk();
		}
		if (pressed(ImGuiMod_Ctrl | ImGuiKey_Comma))
		{
			showSettings = true;
		}
		if (pressed(ImGuiMod_Ctrl | ImGuiKey_Q))
		{
			glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
		}
		if (pressed(ImGuiMod_Shift | ImGuiKey_Escape))
		{
			state.showLog = !state.showLog;
		}

		// The view toggles of LegacyModelView
		// Move and rotate gizmo of the selected tag or hitbox
		if (pressed(ImGuiKey_G))
		{
			view.gizmoMode = GizmoMode::Move;
		}
		if (pressed(ImGuiKey_R))
		{
			view.gizmoMode = GizmoMode::Rotate;
		}
		if (pressed(ImGuiKey_S))
		{
			view.gizmoMode = GizmoMode::Scale;
		}

		// Escape deselects the tag or hitbox and hides the gizmo (dialogs use it to close)
		if (pressed(ImGuiKey_Escape) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && view.editTarget != EditTarget::None)
		{
			view.editTarget = EditTarget::None;
			view.selectedTag = -1;
			view.selectedHitbox = -1;
		}
		if (pressed(ImGuiKey_W))
		{
			view.wireframe = !view.wireframe;
		}
		if (pressed(ImGuiKey_T))
		{
			view.textures = !view.textures;
		}
		if (pressed(ImGuiKey_L))
		{
			view.dynamicLod = !view.dynamicLod;
		}
		if (pressed(ImGuiKey_N))
		{
			state.playback.interpolate = !state.playback.interpolate;
		}
		if (pressed(ImGuiMod_Ctrl | ImGuiKey_B))
		{
			view.drawBounds = !view.drawBounds;
		}
		if (pressed(ImGuiMod_Alt | ImGuiKey_O))
		{
			view.drawGrid = !view.drawGrid;
		}
		if (pressed(ImGuiMod_Alt | ImGuiKey_T))
		{
			view.drawTags = !view.drawTags;
		}
		if (pressed(ImGuiMod_Alt | ImGuiKey_S))
		{
			view.drawSkeleton = !view.drawSkeleton;
		}
		if (pressed(ImGuiMod_Alt | ImGuiKey_N))
		{
			view.drawNormals = !view.drawNormals;
		}

		if (pressed(ImGuiKey_F) && state.model)
		{
			state.frameCamera = true;
		}
		if (pressed(ImGuiKey_Space) && state.model && state.model->hasFrames())
		{
			if (state.playback.isPlaying())
			{
				state.playback.pause();
			}
			else
			{
				if (state.model->currentFrame < 0)
				{
					state.model->currentFrame = state.playback.first();
				}
				state.playback.seek(state.model->currentFrame);
				state.playback.play();
			}
		}
	}
}
