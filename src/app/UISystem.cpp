#include "UISystem.h"

#include "imgui.h"
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

	static Ref<ModelPropertyPanel> propertyPanel;
	static Ref<ScenePanel> scenePanel;

	bool UISystem::setup(GLWindow *window, const char *glslVersion)
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

		propertyPanel = make_ref<ModelPropertyPanel>();
		scenePanel = make_ref<ScenePanel>();

		return true;
	}

	void UISystem::draw()
	{
#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_NewFrame();
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_NewFrame();
#endif

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Enable the whole viewport docking
		ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		// static bool wasRun = false;
		// if (!wasRun)
		// {
		// 	ImGui::DockBuilderRemoveNode(dockspace_id);
		// 	wasRun = true;
		// }

		if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
		{
			ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
			ImGui::DockBuilderSetNodeSize(dockspace_id, { 500,500 });

			ImGuiID dock_main_id = dockspace_id; // This variable will track the document node, however we are not using it here as we aren't docking anything into it.
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
			ImGuiID dock_id_center = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 1.f, nullptr, &dock_main_id);
			ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, nullptr, &dock_main_id);

			ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Properties", dock_left_id);
			ImGui::DockBuilderDockWindow("Mesh", dock_left_id);
			ImGui::DockBuilderDockWindow("Extra", dock_left_id);
			ImGui::DockBuilderDockWindow("Scene", dock_id_center);
			ImGui::DockBuilderFinish(dockspace_id);
		}

		// Setup the main menu bar.
		if (ImGui::BeginMainMenuBar())
		{

			if (ImGui::BeginMenu("File"))
			{
				ImGui::Separator();

				if (ImGui::MenuItem("Quit", "Cmd+Q", false, false))
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
				ImGui::Separator();

				if (ImGui::MenuItem("Close", nullptr, false, false))
				{

				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit", nullptr, false, false))
				{

				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		scenePanel->render();

		propertyPanel->render();

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

	void UISystem::shutdown()
	{
#if defined(MT_OPENGL_2)
		ImGui_ImplOpenGL2_Shutdown();
#elif defined(MT_OPENGL_3)
		ImGui_ImplOpenGL3_Shutdown();
#endif
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}