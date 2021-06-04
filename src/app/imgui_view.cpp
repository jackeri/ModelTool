#include "imgui_view.h"
#include "gl_window.h"
#include "state.h"
#include "hitbox.h"

// Dockspace internal api is needed
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW 1

#if defined(MT_OPENGL_2)

#include "backends/imgui_impl_opengl2.h"

#elif defined(MT_OPENGL_3)
#include "backends/imgui_impl_opengl3.h"
#endif

namespace mt {

	const char *LOAD_MODEL = "Load model";
	const char *LOAD_ANIMATION = "Load animation";

	bool ImGuiView::setup(GLWindow *window, const char *glslVersion)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		(void) io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
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

#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_Init();
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_Init(glslVersion);
#endif
		return true;
	}

	void ImGuiView::startFrame()
	{
#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_NewFrame();
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_NewFrame();
#endif

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiView::endFrame()
	{
		ImGui::Render();

#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

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
		ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
		{
			ImGui::DockBuilderRemoveNode(dockspaceId); // Clear out existing layout
			ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace); // Add empty node
			ImGui::DockBuilderSetNodeSize(dockspaceId, {500, 500});

			ImGuiID dock_main_id = dockspaceId; // This variable will track the document node, however we are not using it here as we aren't docking anything into it.
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr,
															   &dock_main_id);
			ImGuiID dock_id_center = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 1.f, nullptr,
																 &dock_main_id);
			ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, nullptr,
																 &dock_main_id);

			ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Properties", dock_left_id);
			ImGui::DockBuilderDockWindow("Mesh", dock_left_id);
			ImGui::DockBuilderDockWindow("Extra", dock_left_id);
			ImGui::DockBuilderDockWindow("Scene", dock_id_center);
			ImGui::DockBuilderFinish(dockspaceId);
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
			browser.show("Select the base folder", [&](const std::string &path) {
				state.filesystem.addPath(path);
			}, true);
		}

		browser.handle();
	}

	void ImGuiView::shutdown()
	{
#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_Shutdown();
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_Shutdown();
#endif
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
						std::cout << "Selected filename" << model << std::endl;
						state.loadModel(model);
					});
					browser.setTypeFilters({".md5mesh"});
				}

				if (ImGui::MenuItem("Load animation", nullptr, false, !!state.model))
				{
					browser.show("Load animation", [&](const std::string &animation) {
						std::cout << "Selected filename" << animation << std::endl;
						state.loadAnimation(animation);
					});
					browser.setTypeFilters({".md5anim"});
				}

				if (ImGui::MenuItem("Load hitboxes", nullptr, false, (!!state.model && state.model->skeletalModel())))
				{
					browser.show("Load hitboxes", [&](const std::string &file) {
						std::cout << "Selected filename" << file << std::endl;
						model::Hitbox::loadHitboxes(state.filesystem.loadFile(file), *state.model->getHitboxes());
					});
					browser.setTypeFilters({".hitboxes"});
				}

				if (ImGui::MenuItem("Save hitboxes", nullptr, false, (!!state.model && state.model->skeletalModel() &&
																	  !state.model->getHitboxes()->empty())))
				{
					browser.show("Load hitboxes", [&](const std::string &file) {
						std::cout << "Selected filename" << file << std::endl;
						model::Hitbox::saveHitboxes(file, *state.model->getHitboxes());
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