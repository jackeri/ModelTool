#include "imgui_view.h"
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
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle &style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window->system(), true);

		ImGui_ImplOpenGL3_Init(glslVersion);
		return true;
	}

	void ImGuiView::startFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
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

			ImGui::DockBuilderDockWindow("Properties", leftId);
			ImGui::DockBuilderDockWindow("Scene", sceneId);
			ImGui::DockBuilderFinish(dockId);
		}

		drawMenu();
		drawScaleBoundsWindow();
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

		// Setup the base path
		if (!state.filesystem.hasSources() && !browser.isOpen())
		{
			browser.show("Select the base folder", [&](const std::string &path) { state.filesystem.addPath(path); }, true);
		}

		browser.handle();
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
				if (ImGui::MenuItem("Open model"))
				{
					browser.show("Load model", [&](const std::string &model) {
						logger->info("Selected filename: {}", model);
						try
						{
							state.loadModel(model);
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load model: {}", ex.what());
						}
					});
					browser.setTypeFilters(model::io::modelExtensions());
				}

				if (ImGui::MenuItem("Load animation", nullptr, false, !!state.model))
				{
					browser.show("Load animation", [&](const std::string &animation) {
						logger->info("Selected filename: {}", animation);
						try
						{
							state.loadAnimation(animation);
						}
						catch (const std::exception &ex)
						{
							logger->error("Failed to load animation: {}", ex.what());
						}
					});
					browser.setTypeFilters(model::io::animationExtensions());
				}

				if (ImGui::MenuItem("Load hitboxes", nullptr, false, (!!state.model && state.model->skeletalModel())))
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
				if (ImGui::MenuItem("Load tags", nullptr, false, skeletal && !skeletal->joints.empty()))
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

				if (ImGui::MenuItem("Save hitboxes", nullptr, false,
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
				if (ImGui::MenuItem("Close", nullptr, false, !!state.model))
				{
					state.closeModels();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit", "Cmd+Q", false, true))
				{
					std::exit(0);
				}

				ImGui::EndMenu();
			}

			drawEditMenu();

			if (ImGui::BeginMenu("View"))
			{
				auto &view = state.view;
				ImGui::MenuItem("Wireframe", nullptr, &view.wireframe);
				ImGui::MenuItem("Textures", nullptr, &view.textures);
				ImGui::MenuItem("Alpha", nullptr, &view.alpha);
				ImGui::Separator();
				ImGui::MenuItem("Grid and origin", nullptr, &view.drawGrid);
				ImGui::MenuItem("Bounding box", nullptr, &view.drawBounds);
				ImGui::MenuItem("Hitboxes", nullptr, &view.drawHitboxes);
				ImGui::MenuItem("Tags", nullptr, &view.drawTags);
				ImGui::MenuItem("Tag names", nullptr, &view.drawTagNames, view.drawTags);
				ImGui::MenuItem("Skeleton", nullptr, &view.drawSkeleton);
				ImGui::MenuItem("Normals", nullptr, &view.drawNormals);
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
				if (ImGui::MenuItem("Reset camera"))
				{
					state.resetCamera = true;
				}
				if (ImGui::MenuItem("Frame model", nullptr, false, !!state.model))
				{
					state.frameCamera = true;
				}

				ImGui::EndMenu();
			}

			// FIXME: implement when we have property loading and persisting
			// if (ImGui::BeginMenu("Options"))
			// {
			// 	// Disabling fullscreen would allow the window to be moved to the front of other windows,
			// 	// which we can't undo at the moment without finer window depth/z control.
			// 	if (ImGui::MenuItem("Fullscreen", nullptr, false))
			// 	{
			//
			// 	}
			// 	if (ImGui::MenuItem("Padding", nullptr, false))
			// 	{
			//
			// 	}
			//
			// 	ImGui::EndMenu();
			// }

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

		if (!ImGui::BeginMenu("Export", model != nullptr))
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
		if (ImGui::MenuItem("Export all..."))
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
}
