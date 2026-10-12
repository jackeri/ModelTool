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
}
