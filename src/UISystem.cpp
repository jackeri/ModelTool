#include "UISystem.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW 1

#include "backends/imgui_impl_opengl3.h"

namespace mt {

	bool UISystem::setup(GLFWwindow *window, const char *glslVersion)
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
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 150");

		return true;
	}

	void UISystem::draw()
	{
		static float f = 0.0f;
		static int counter = 0;
		static bool show_demo_window = true;
		static bool show_another_window = false;
		static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		if (true)
		{
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

			ImGuiWindowFlags window_flags = 0; //ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

			// Create a window called "Hello, world!" and append into it.
			ImGui::Begin("Hello, world!", nullptr, window_flags);

			ImGui::Text(
					"This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float *) &clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))
			{                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			}
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
						ImGui::GetIO().Framerate);

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Options"))
				{
					// Disabling fullscreen would allow the window to be moved to the front of other windows,
					// which we can't undo at the moment without finer window depth/z control.
					if (ImGui::MenuItem("Fullscreen", NULL, false))
					{

					}
					if (ImGui::MenuItem("Padding", NULL, false))
					{

					}
					ImGui::Separator();

					if (ImGui::MenuItem("Close", NULL, false, false))
					{

					}
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			ImGui::End();
		}

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

	void UISystem::shutdown()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}