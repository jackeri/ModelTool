#include "model_propertypanel.h"
#include "state.h"
#include "skeletal_model.h"
#include "hitbox.h"
#include "morph_model.h"

using namespace mt::model;
using namespace mt::model::skeletal;

namespace mt {
	void ModelPropertyPanel::render()
	{
		auto &state = singleton<State>();

		if (ImGui::Begin("Properties", nullptr, 0))
		{
			if (!state.model)
			{
				ImGui::Text("No model loaded");
			}
			else if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
			{
				drawModelBasics();
			}

			if (state.model && ImGui::CollapsingHeader("Transform"))
			{
				drawTransformMenu();
			}

			if (state.model && state.model->hasFrames())
			{
				if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
				{
					drawAnimationMenu();
				}
			}

			if (state.model && ImGui::CollapsingHeader("Animation sequences"))
			{
				drawSequencesMenu();
			}

			if (state.model && ImGui::CollapsingHeader("Meshes"))
			{
				drawMeshesMenu();
			}

			if (state.model && ImGui::CollapsingHeader("Tags"))
			{
				drawTagsMenu();
			}

			if (state.model && state.model->skeletalModel() && ImGui::CollapsingHeader("Bones"))
			{
				drawBonesMenu();
			}

			if (state.model)
			{
				if (ImGui::CollapsingHeader("Level of detail"))
				{
					drawLodMenu();
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
				ImGui::ColorEdit3("background color", (float *)&state.clearColor);

				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			}
		}
		ImGui::End();
	}

	void ModelPropertyPanel::drawAnimationMenu()
	{
		auto &state = singleton<State>();
		auto &playback = state.playback;
		Model &model = *state.model;

		if (ImGui::Button(playback.isPlaying() ? "Pause" : "Play"))
		{
			if (playback.isPlaying())
			{
				playback.pause();
			}
			else
			{
				if (model.currentFrame < 0)
				{
					model.currentFrame = playback.first();
				}
				playback.seek(model.currentFrame);
				playback.play();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("All frames"))
		{
			state.resetPlayback();
		}

		ImGui::SliderFloat("Speed", &playback.speed, 0.0f, 4.0f, "%.2fx");
		ImGui::Checkbox("Interpolation", &playback.interpolate);
		ImGui::SameLine();
		ImGui::Checkbox("Loop", &playback.loop);

		int first = playback.first(), last = playback.last();
		if (ImGui::DragIntRange2("Frame range", &first, &last, 1, 0, model.numFrames() - 1))
		{
			playback.setRange(first, std::max(first, last), 0, playback.loop);
		}
	}

	void ModelPropertyPanel::drawLodMenu()
	{
		auto &state = singleton<State>();
		auto &view = state.view;

		if (auto *morph = dynamic_cast<model::morph::MorphModel *>(state.model.get()); morph && morph->lodCount() > 1)
		{
			ImGui::SliderInt("Level", &view.lodLevel, 0, morph->lodCount() - 1);
		}

		ImGui::Checkbox("Dynamic LOD (collapse maps)", &view.dynamicLod);
		if (view.dynamicLod)
		{
			ImGui::SliderFloat("Detail", &view.lodRatio, 0.0f, 1.0f, "%.2f");
			ImGui::TextDisabled("0 = the mesh's minimum LOD, 1 = full mesh");
		}
	}

	void ModelPropertyPanel::drawHitboxMenu()
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

			ImGui::Combo(
				"Bind bone", &hitbox.parentJoint,
				[](void *data, int idx, const char **out_text) -> bool {
					auto *list = (std::vector<Joint> *)data;

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

			bool headshot
				= std::any_of(hitbox.modifiers.begin(), hitbox.modifiers.end(), [](auto &modifier) { return modifier == "headshot"; });
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
}
