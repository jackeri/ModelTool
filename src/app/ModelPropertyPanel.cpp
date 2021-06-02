#include "ModelPropertyPanel.h"
#include "state.h"
#include "SkeletalModel.h"

using namespace mt::model::Skeletal;

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

		if (state.model && state.model->skeletalModel())
		{
			if (ImGui::CollapsingHeader("Hitboxes"))
			{
				drawHitboxMenu();
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

void mt::ModelPropertyPanel::drawHitboxMenu()
{
	auto &state = singleton<State>();

	auto *model = dynamic_cast<SkeletalModel *>(state.model.get());

	ImGui::Text("Number of Hitboxes: %lu", model->hitboxes.size());
	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		Hitbox tmp(10);
		model->hitboxes.push_back(tmp);
	}

	for (unsigned int i = 0; i < model->hitboxes.size(); i++)
	{
		Hitbox &hitbox = model->hitboxes[i];

		ImGui::PushID(BIT(23u) + i);
		ImGui::Separator();
		ImGui::InputText("Name", &hitbox.name);

		ImGui::Spacing();

		ImGui::Combo("Bind bone", &hitbox.parentJoint,
					 [](void *data, int idx, const char **out_text) -> bool {
						 auto *list = (std::vector<Joint> *) data;

						 if (idx < 0 || idx >= list->size())
						 {
							 out_text = nullptr;
						 }
						 else
						 {
							 *out_text = list->at(idx).name.c_str();
						 }

						 return true;
					 },
					 &model->joints, model->joints.size());

		ImGui::Spacing();

		bool headshot = std::any_of(hitbox.modifiers.begin(), hitbox.modifiers.end(),
									[](auto &modifier) { return modifier == "headshot"; });
		ImGui::Checkbox("Head", &headshot);

		if (!headshot)
		{
			hitbox.modifiers.erase("headshot");
		}
		else
		{
			hitbox.modifiers.emplace("headshot");
		}

		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Rotation"))
		{
			// Returns euler angles, pitch as x, yaw as y, roll as z.
			glm::vec3 angles = glm::degrees(glm::eulerAngles(hitbox.rotation));
			ImGui::SliderFloat("Roll (X)", &angles.z, -180.f, 180.f);
			ImGui::SliderFloat("Pitch (Y)", &angles.x, -180.f, 180.f);
			ImGui::SliderFloat("Yawn (Z)", &angles.y, -180.f, 180.f);
			hitbox.rotation = {glm::radians(angles)};
		}

		if (ImGui::CollapsingHeader("Translation"))
		{
			ImGui::SliderFloat("Trans X", &hitbox.location.x, -180.f, 180.f);
			ImGui::SliderFloat("Trans Y", &hitbox.location.y, -180.f, 180.f);
			ImGui::SliderFloat("Trans Z", &hitbox.location.z, -180.f, 180.f);
		}

		if (ImGui::CollapsingHeader("Size"))
		{
			glm::vec3 tmp = hitbox.max - hitbox.min;

			ImGui::SliderFloat("Height", &tmp.x, 1, 180.f);
			ImGui::SliderFloat("Width", &tmp.z, 1, 180.f);
			ImGui::SliderFloat("Depth", &tmp.y, 1, 180.f);

			hitbox.min = {-(tmp.x / 2), -(tmp.y / 2), -(tmp.z / 2)};
			hitbox.max = {tmp.x / 2, tmp.y / 2, tmp.z / 2};
		}

		if (ImGui::Button("Delete"))
		{
			model->hitboxes.erase(model->hitboxes.begin() + i);
		}

		ImGui::PopID();
	}
}
