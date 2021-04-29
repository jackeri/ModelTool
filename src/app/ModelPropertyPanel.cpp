#include "ModelPropertyPanel.h"

#include "imgui.h"
#include "state.h"

mt::ModelPropertyPanel::ModelPropertyPanel()
{
}

mt::ModelPropertyPanel::~ModelPropertyPanel()
{

}

void mt::ModelPropertyPanel::render()
{
	auto &state = singleton<State>();

	ImGui::Begin("Properties", nullptr, 0);

	ImGui::ColorEdit3("background color", (float *) &state.clearColor);
	
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
				ImGui::GetIO().Framerate);

	ImGui::End();
}
