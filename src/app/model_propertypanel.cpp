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
			// Every section has its own ID scope, so the same labels (like "Add") can be used in several sections
			auto section = [](const char *label, ImGuiTreeNodeFlags flags, const std::function<void()> &draw) {
				if (ImGui::CollapsingHeader(label, flags))
				{
					ImGui::PushID(label);
					draw();
					ImGui::PopID();
				}
			};
			const bool model = !!state.model;
			const bool skeletal = model && state.model->skeletalModel();

			if (!model)
			{
				ImGui::Text("No model loaded");
			}
			else
			{
				section("Model", ImGuiTreeNodeFlags_DefaultOpen, [&] { drawModelBasics(); });
			}
			if (state.isPlayer)
			{
				section("Player", ImGuiTreeNodeFlags_DefaultOpen, [&] { drawPlayerMenu(); });
			}
			if (model)
			{
				section("Transform", 0, [&] { drawTransformMenu(); });
			}
			if (model && state.model->hasFrames())
			{
				section("Animation", ImGuiTreeNodeFlags_DefaultOpen, [&] { drawAnimationMenu(); });
			}
			if (model)
			{
				section("Animation sequences", 0, [&] { drawSequencesMenu(); });
				section("Meshes", 0, [&] { drawMeshesMenu(); });
				section("Tags", 0, [&] { drawTagsMenu(); });
				section("Attachments", 0, [&] { drawAttachmentsMenu(); });
			}
			if (skeletal)
			{
				section("Bones", 0, [&] { drawBonesMenu(); });
			}
			if (model)
			{
				section("Level of detail", 0, [&] { drawLodMenu(); });
			}
			if (skeletal)
			{
				section("Hitboxes", 0, [&] { drawHitboxMenu(); });
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

	namespace {
		/**
		 * Sequence combo, the selected sequence is the one the playback plays
		 * @return the picked sequence, nullptr if nothing was picked
		 */
		const Animation *sequenceCombo(const char *label, const std::vector<Animation> &list, const model::Playback &playback)
		{
			const Animation *current = nullptr;
			for (const auto &animation : list)
			{
				if (animation.startFrame == playback.first() && animation.lastFrame() == playback.last())
				{
					current = &animation;
				}
			}

			const Animation *picked = nullptr;
			if (ImGui::BeginCombo(label, current ? current->name.c_str() : "(frames)", ImGuiComboFlags_HeightLarge))
			{
				for (const auto &animation : list)
				{
					if (ImGui::Selectable(animation.name.c_str(), &animation == current))
					{
						picked = &animation;
					}
				}
				ImGui::EndCombo();
			}
			return picked;
		}
	}

	void ModelPropertyPanel::drawPlayerMenu()
	{
		auto &state = singleton<State>();
		if (state.attachments.empty())
		{
			return;
		}

		if (ImGui::BeginCombo("Skin", state.playerSkin.empty() ? "(none)" : state.playerSkin.c_str()))
		{
			for (const auto &name : state.player.skins)
			{
				if (ImGui::Selectable(name.c_str(), name == state.playerSkin))
				{
					state.setPlayerSkin(name);
				}
			}
			ImGui::EndCombo();
		}

		// BOTH_* sequences (deaths) are played by the legs and the torso together
		Attachment &upper = state.attachments[0];
		auto playBoth = [&](const Animation &animation, bool legs) {
			state.playSequence(legs ? -1 : 0, animation);
			if (tools::startsWith(animation.name, "BOTH_"))
			{
				const auto &other = legs ? upper.model->animations : state.model->animations;
				auto same = std::find_if(other.begin(), other.end(), [&](const auto &item) { return item.name == animation.name; });
				if (same != other.end())
				{
					state.playSequence(legs ? 0 : -1, *same);
				}
			}
		};

		if (const Animation *legs = sequenceCombo("Legs", state.model->animations, state.playback))
		{
			playBoth(*legs, true);
		}
		if (const Animation *torso = sequenceCombo("Torso", upper.model->animations, upper.playback))
		{
			playBoth(*torso, false);
		}

		ImGui::TextDisabled("Sex %s, footsteps %s", state.playerConfig.sex.c_str(), state.playerConfig.footsteps.c_str());
		ImGui::TextDisabled("Play and pause in the Animation section");
	}

	void ModelPropertyPanel::drawAttachmentsMenu()
	{
		auto &state = singleton<State>();

		if (state.attachments.empty())
		{
			ImGui::TextWrapped("Attach a model with the Attach button of a tag (Tags) or a bone (Bones).");
			return;
		}

		int remove = -1;
		for (int i = 0; i < int(state.attachments.size()); i++)
		{
			Attachment &attachment = state.attachments[i];
			ImGui::PushID(i);
			if (ImGui::TreeNodeEx("attachment", ImGuiTreeNodeFlags_DefaultOpen, "%s", attachment.name.c_str()))
			{
				// Parents come before their children in the list
				auto parentName = [&](int index) { return index < 0 ? std::string("main model") : state.attachments[index].name; };
				if (ImGui::BeginCombo("On", parentName(attachment.parent).c_str()))
				{
					for (int parent = -1; parent < i; parent++)
					{
						ImGui::PushID(parent);
						if (ImGui::Selectable(parentName(parent).c_str(), parent == attachment.parent))
						{
							attachment.parent = parent;
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}

				const auto tags = state.attachPoints(attachment.parent);
				const bool tagFound = std::find(tags.begin(), tags.end(), attachment.tag) != tags.end();
				if (ImGui::BeginCombo("At", attachment.tag.c_str()))
				{
					for (const auto &tag : tags)
					{
						if (ImGui::Selectable(tag.c_str(), tag == attachment.tag))
						{
							attachment.tag = tag;
						}
					}
					ImGui::EndCombo();
				}
				if (!tagFound)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1, 0.6f, 0.2f, 1});
					ImGui::TextWrapped("The parent has no tag or bone %s, drawn at its origin", attachment.tag.c_str());
					ImGui::PopStyleColor();
				}

				if (attachment.model->numFrames() > 1)
				{
					if (!attachment.model->animations.empty())
					{
						if (const Animation *picked = sequenceCombo("Sequence", attachment.model->animations, attachment.playback))
						{
							state.playSequence(i, *picked);
						}
					}
					int frame = std::max(attachment.model->currentFrame, 0);
					if (ImGui::SliderInt("Frame", &frame, 0, attachment.model->numFrames() - 1))
					{
						attachment.playback.seek(frame);
						attachment.model->currentFrame = frame;
					}
				}

				// Models can be attached to the attached model's own tags and bones too
				const auto points = state.attachPoints(i);
				if (!points.empty() && ImGui::TreeNode("points", "Tags and bones (%zu)", points.size()))
				{
					for (const auto &point : points)
					{
						ui::attachButton(i, point);
						ImGui::SameLine();
						ImGui::TextUnformatted(point.c_str());
					}
					ImGui::TreePop();
				}

				if (ImGui::Button(ICON_FA_TRASH " Remove"))
				{
					remove = i;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (remove >= 0)
		{
			state.removeAttachment(remove);
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
		// No hitbox left to edit (deleted, or another model)
		if (selected < 0 && state.view.editTarget == EditTarget::Hitbox)
		{
			state.view.editTarget = EditTarget::None;
		}

		ImGui::Text("Hitboxes: %zu", hitboxes.size());
		if (ImGui::Button(ICON_FA_PLUS " Add"))
		{
			Hitbox hitbox(10);
			hitbox.name = "hitbox " + std::to_string(hitboxes.size() + 1);
			hitboxes.push_back(hitbox);
			selected = int(hitboxes.size()) - 1;
			state.view.editTarget = EditTarget::Hitbox;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(selected < 0);
		if (ImGui::Button(ICON_FA_CLONE " Duplicate"))
		{
			Hitbox copy = hitboxes[selected];
			copy.name += " copy";
			hitboxes.insert(hitboxes.begin() + selected + 1, copy);
			selected++;
			state.view.editTarget = EditTarget::Hitbox;
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
					state.view.editTarget = selected >= 0 ? EditTarget::Hitbox : EditTarget::None;
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
