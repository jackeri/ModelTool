#include "model_propertypanel.h"
#include "state.h"
#include "skeletal_model.h"
#include "hitbox.h"
#include "morph_model.h"
#include "ui_widgets.h"
#include "icons_font_awesome.h"

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
				int theme = int(state.theme);
				if (ImGui::Combo("Theme", &theme, "Darker\0Dark\0Light\0"))
				{
					// The scene background follows the theme, it can be changed after that
					state.theme = ui::Theme(theme);
					glm::vec3 background = ui::sceneBackground(state.theme);
					state.clearColor = ImColor(background.r, background.g, background.b);
				}

				// The font atlas is rebuilt when the slider is let go
				static float fontSize = 0;
				if (!ImGui::IsAnyItemActive() || fontSize == 0)
				{
					fontSize = state.fontSize;
				}
				ImGui::SliderFloat("Font size", &fontSize, ui::MIN_FONT_SIZE, ui::MAX_FONT_SIZE, "%.0f");
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					state.fontSize = std::round(fontSize);
				}

				// Leave room for the reset button next to the color
				const float resetWidth = ImGui::CalcTextSize(ICON_FA_UNDO).x + ImGui::GetStyle().FramePadding.x * 2;
				ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - resetWidth - ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::ColorEdit3("##background", (float *)&state.clearColor);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button(ICON_FA_UNDO "##background"))
				{
					glm::vec3 background = ui::sceneBackground(state.theme);
					state.clearColor = ImColor(background.r, background.g, background.b);
				}
				ImGui::SetItemTooltip("Theme default");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::TextUnformatted("Background");

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

		if (ImGui::Button(playback.isPlaying() ? ICON_FA_PAUSE " Pause" : ICON_FA_PLAY " Play"))
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
		if (ImGui::Button(ICON_FA_REFRESH " All frames"))
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
		auto &hitboxes = model->hitboxes;
		int &selected = state.view.selectedHitbox;
		if (selected >= int(hitboxes.size()))
		{
			selected = int(hitboxes.size()) - 1;
		}

		ImGui::Text("Hitboxes: %zu", hitboxes.size());
		if (ImGui::Button(ICON_FA_PLUS " Add"))
		{
			Hitbox hitbox(10);
			hitbox.name = "hitbox " + std::to_string(hitboxes.size() + 1);
			hitboxes.push_back(hitbox);
			selected = int(hitboxes.size()) - 1;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(selected < 0);
		if (ImGui::Button(ICON_FA_CLONE " Duplicate"))
		{
			Hitbox copy = hitboxes[selected];
			copy.name += " copy";
			hitboxes.insert(hitboxes.begin() + selected + 1, copy);
			selected++;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TRASH " Delete"))
		{
			hitboxes.erase(hitboxes.begin() + selected);
			selected = std::min(selected, int(hitboxes.size()) - 1);
		}
		ImGui::EndDisabled();

		if (ImGui::BeginListBox("##hitboxes",
								{-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * float(std::clamp<size_t>(hitboxes.size(), 3, 8))}))
		{
			for (int i = 0; i < int(hitboxes.size()); i++)
			{
				ImGui::PushID(i);
				std::string label = hitboxes[i].name.empty() ? "(no name)" : hitboxes[i].name;
				label += std::string("  [") + hitboxShapeName(hitboxes[i].shape) + "]";
				if (ImGui::Selectable(label.c_str(), selected == i))
				{
					selected = selected == i ? -1 : i;
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		if (selected < 0)
		{
			ImGui::TextDisabled("Select a hitbox to edit it");
			return;
		}

		Hitbox &hitbox = hitboxes[selected];
		ImGui::PushID(selected);
		ImGui::SeparatorText("Hitbox");
		ImGui::InputText("Name", &hitbox.name);
		ui::boneCombo("Bone", model->joints, hitbox.parentJoint, true);

		bool headshot = hitbox.modifiers.count("headshot") > 0;
		if (ImGui::Checkbox("Head", &headshot))
		{
			if (headshot)
			{
				hitbox.modifiers.emplace("headshot");
			}
			else
			{
				hitbox.modifiers.erase("headshot");
			}
		}

		int shape = int(hitbox.shape);
		const char *shapes[] = {"Box", "Sphere", "Cylinder", "Capsule"};
		if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
		{
			hitbox.setShape(HitboxShape(shape));
		}

		ImGui::DragFloat3("Location", glm::value_ptr(hitbox.location), 0.1f, 0, 0, "%.2f");
		ui::editRotation("Rotation", hitbox.rotation);

		if (hitbox.shape == HitboxShape::Box)
		{
			// The size keeps the centre of the box where it is
			glm::vec3 size = hitbox.max - hitbox.min;
			glm::vec3 center = (hitbox.max + hitbox.min) * 0.5f;
			bool changed = ImGui::DragFloat3("Size", glm::value_ptr(size), 0.1f, 0.0f, FLT_MAX, "%.2f");
			changed |= ImGui::DragFloat3("Center", glm::value_ptr(center), 0.1f, 0, 0, "%.2f");
			if (changed)
			{
				size = glm::max(size, glm::vec3(0));
				hitbox.min = center - size * 0.5f;
				hitbox.max = center + size * 0.5f;
			}
		}
		else
		{
			bool changed = ImGui::DragFloat("Radius", &hitbox.radius, 0.1f, 0.0f, FLT_MAX, "%.2f");
			if (hitbox.shape != HitboxShape::Sphere)
			{
				changed |= ImGui::DragFloat("Height", &hitbox.height, 0.1f, 0.0f, FLT_MAX, "%.2f");
				if (hitbox.shape == HitboxShape::Capsule)
				{
					ImGui::TextDisabled("The height includes the end caps");
				}
			}
			if (changed)
			{
				hitbox.clampSize();
			}
		}
		ImGui::PopID();
	}
}
