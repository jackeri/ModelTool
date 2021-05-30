#include "ImGuiView.h"

#include "GLWindow.h"

// Dockspace internal api is needed
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW 1

#if defined(MT_OPENGL_2)

#include "backends/imgui_impl_opengl2.h"

#elif defined(MT_OPENGL_3)
#include "backends/imgui_impl_opengl3.h"
#endif

#include "imfilebrowser.h"
#include "state.h"
#include "ModelLoader.h"

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

		if (!m_lateExecution.empty()) {
			for (auto &exec : m_lateExecution) {
				exec();
			}
			m_lateExecution.clear();
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal(LOAD_MODEL, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{

			ImGui::Text("Test");

			ImGui::EndPopup();
		}


		auto layerIterator = m_uiLayers.begin();
		while (layerIterator != m_uiLayers.end())
		{
			bool layerOk = true;
			layerIterator->second(layerOk);

			if (!layerOk)
			{
				layerIterator = m_uiLayers.erase(layerIterator);
			}
			else
			{
				layerIterator++;
			}
		}

		// Setup the base path
		if (!state.filesystem.hasSources() && !fileBrowser.IsOpened())
		{
			fileBrowser.SetTitle("Select the base folder");
			fileBrowser.Open();
		}

		fileBrowser.Display();

		if (fileBrowser.HasSelected())
		{
			std::cout << "Selected filename" << fileBrowser.GetSelected().string() << std::endl;
			state.filesystem.addPath(fileBrowser.GetSelected());
			fileBrowser.ClearSelected();
		}
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
					m_lateExecution.emplace_back([&]() {
						// ImGui::OpenPopup(LOAD_MODEL, ImGuiWindowFlags_AlwaysAutoResize);
						state.loadModel("models/md5/bob_lamp_update_export.md5mesh");
					});
				}

				if (ImGui::MenuItem("Load animation", nullptr, false, !!state.model))
				{
					m_lateExecution.emplace_back([&]() {
						// ImGui::OpenPopup(LOAD_ANIMATION, ImGuiWindowFlags_AlwaysAutoResize);
						state.loadAnimation("models/md5/bob_lamp_update_export.md5anim");
					});
				}

				// Close the current model
				if (ImGui::MenuItem("Close", nullptr, false, !state.models.empty()))
				{
					// FIXME: implement
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit", "Cmd+Q", false, true))
				{
					// Emit quit message
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Options"))
			{
				// Disabling fullscreen would allow the window to be moved to the front of other windows,
				// which we can't undo at the moment without finer window depth/z control.
				if (ImGui::MenuItem("Fullscreen", nullptr, false))
				{

				}
				if (ImGui::MenuItem("Padding", nullptr, false))
				{

				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}
}