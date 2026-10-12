#include "imgui_view.h"
#include "gl_window.h"
#include "state.h"
#include "hitbox.h"
#include "scene_panel.h"
#include "texture.h"
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
}
