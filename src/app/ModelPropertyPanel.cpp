#include "ModelPropertyPanel.h"

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

	if (ImGui::Begin("Properties", nullptr, 0))
	{
		if (ImGui::CollapsingHeader("Model basics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (state.model)
			{
				ImGui::InputText("Name", &state.model->name);

				// Draw frame selector if the model has frames available
				if (state.model->hasFrames())
				{
					ImGui::SliderInt("Frame", &state.model->currentFrame, mt::FRAME_BIND_POSE,
									 state.model->numFrames() - 1);
				}
			}
			else
			{
				ImGui::Text("No model loaded");
			}
		}

		if (ImGui::CollapsingHeader("App", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::ColorEdit3("background color", (float *) &state.clearColor);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
						ImGui::GetIO().Framerate);
		}

		ImGui::End();
	}
}
