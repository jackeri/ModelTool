#include "ModelPropertyPanel.h"

#include "imgui.h"

mt::ModelPropertyPanel::ModelPropertyPanel()
{
}

mt::ModelPropertyPanel::~ModelPropertyPanel()
{

}

void mt::ModelPropertyPanel::render()
{
	static float f = 0.0f;
	static int counter = 0;
	static bool show_demo_window = true;
	static bool show_another_window = false;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGuiWindowFlags window_flags = 0; //ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	// Create a window called "Hello, world!" and append into it.
	ImGui::Begin("Properties", nullptr, window_flags);

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
