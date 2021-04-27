#include "ScenePanel.h"

#include "imgui.h"

mt::ScenePanel::ScenePanel()
{

}

mt::ScenePanel::~ScenePanel()
{

}

void mt::ScenePanel::render()
{
	ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;

	ImGui::Begin("Scene", nullptr, sceneFlags);

	ImGui::Text("Jeppis jepulis");

	if (ImGui::Button("test"))
	{
		// Yep
	}

	ImGui::End();
}
