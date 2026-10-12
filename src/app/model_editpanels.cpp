#include "model_propertypanel.h"
#include "state.h"
#include "skeletal_model.h"
#include "skeletal_tools.h"
#include "morph_model.h"
#include "ui_widgets.h"
#include "icons_font_awesome.h"

using namespace mt::model;
using namespace mt::model::skeletal;
using namespace mt::ui;

namespace mt {

	void ModelPropertyPanel::drawModelBasics()
	{
		auto &state = singleton<State>();
		Model &model = *state.model;
		auto *skeletal = dynamic_cast<SkeletalModel *>(&model);

		ImGui::InputText("Name", &model.name);

		if (!model.hasFrames())
		{
			ImGui::TextDisabled("No animation frames");
			return;
		}

		const int lastFrame = model.numFrames() - 1;
		const bool hasBindPose = skeletal && skeletal->hasBindPose;

		if (hasBindPose)
		{
			bool bindPose = model.currentFrame == FRAME_BIND_POSE;
			if (ImGui::Checkbox("Show bind pose", &bindPose))
			{
				state.playback.pause();
				model.currentFrame = bindPose ? FRAME_BIND_POSE : std::clamp(state.playback.frame(), 0, lastFrame);
			}
		}

		if (model.currentFrame != FRAME_BIND_POSE || !hasBindPose)
		{
			int frame = std::max(model.currentFrame, 0);
			if (ImGui::SliderInt("Frame", &frame, 0, lastFrame))
			{
				model.currentFrame = frame;
				state.playback.pause();
				state.playback.seek(frame);
			}
		}

		int first = model.firstExportFrame;
		int last = model.lastExportFrame < 0 ? lastFrame : model.lastExportFrame;
		if (ImGui::DragIntRange2("Export frames", &first, &last, 1, 0, lastFrame))
		{
			model.firstExportFrame = std::clamp(first, 0, lastFrame);
			model.lastExportFrame = std::clamp(last, model.firstExportFrame, lastFrame);
		}

		if (skeletal)
		{
			ImGui::Checkbox("Flip bone tags", &skeletal->flipBoneTags);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Turns the tags made from bones (needed for MS3D models)");
			}

			if (model.currentFrame >= 0)
			{
				if (ImGui::Button(ICON_FA_CHECK " Bake pose"))
				{
					bakeBindPose(*skeletal, model.currentFrame);
					spdlog::get(MT_LOGGER)->info("Frame {} is now the bind pose", model.currentFrame);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Make this frame the bind pose");
				}
			}
		}
	}

	void ModelPropertyPanel::drawTransformMenu()
	{
		Model &model = *singleton<State>().model;

		ImGui::DragFloat("Scale", &model.scale, 0.01f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		editRotation("Rotation", model.transform.rotation);
		ImGui::DragFloat3("Translation", glm::value_ptr(model.transform.location), 0.1f);

		if (ImGui::Button(ICON_FA_UNDO " Reset transform"))
		{
			model.scale = 1.0f;
			model.transform = Point{};
		}
		ImGui::TextDisabled("The transform is applied when exporting");
	}

	void ModelPropertyPanel::drawMeshesMenu()
	{
		auto &state = singleton<State>();

		auto meshRow = [](int index, std::string &name, std::string &material, int &minLod, size_t vertices, size_t tris) {
			ImGui::PushID(index);
			if (ImGui::TreeNode("mesh", "%s", name.empty() ? "(unnamed)" : name.c_str()))
			{
				ImGui::InputText("Name", &name);
				ImGui::InputText("Material", &material);
				ImGui::Text("%zu vertices, %zu triangles", vertices, tris);
				ImGui::SliderInt("Minimum LOD", &minLod, 0, int(vertices));
				ImGui::TreePop();
			}
			ImGui::PopID();
		};

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(state.model.get()))
		{
			ImGui::Text("Meshes: %zu", skeletal->meshes.size());
			for (size_t i = 0; i < skeletal->meshes.size(); i++)
			{
				auto &mesh = skeletal->meshes[i];
				meshRow(int(i), mesh.name, mesh.materialName, mesh.minLod, mesh.vertices.size(), mesh.tris.size());
			}
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(state.model.get()))
		{
			ImGui::Text("Meshes: %zu", morph->meshes.size());
			for (size_t i = 0; i < morph->meshes.size(); i++)
			{
				auto &mesh = morph->meshes[i];
				meshRow(int(i), mesh.name, mesh.materialName, mesh.minLod, mesh.numVertices(), mesh.tris.size());
			}
		}
	}

	void ModelPropertyPanel::drawTagsMenu()
	{
		auto &state = singleton<State>();

		if (auto *skeletal = dynamic_cast<SkeletalModel *>(state.model.get()))
		{
			ImGui::Text("Tags: %zu", skeletal->tags.size());
			ImGui::SameLine();
			if (ImGui::SmallButton(ICON_FA_PLUS " Add") && !skeletal->joints.empty())
			{
				BoneTag tag;
				tag.name = "tag_new";
				tag.parentBone = 0;
				skeletal->tags.push_back(tag);
			}

			for (size_t i = 0; i < skeletal->tags.size(); i++)
			{
				auto &tag = skeletal->tags[i];
				ImGui::PushID(int(i));
				if (ImGui::TreeNode("tag", "%s", tag.name.c_str()))
				{
					ImGui::InputText("Name", &tag.name);
					boneCombo("Bone", skeletal->joints, tag.parentBone, false);
					ImGui::DragFloat3("Location", glm::value_ptr(tag.location), 0.1f);
					editRotation("Rotation", tag.rotation);
					ui::attachButton(-1, tag.name);
					ImGui::SameLine();
					if (ImGui::Button(ICON_FA_TRASH " Remove tag"))
					{
						skeletal->tags.erase(skeletal->tags.begin() + long(i));
						ImGui::TreePop();
						ImGui::PopID();
						break;
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
		else if (auto *morph = dynamic_cast<morph::MorphModel *>(state.model.get()))
		{
			ImGui::Text("Tags: %zu", morph->tags.size());
			for (size_t i = 0; i < morph->tags.size(); i++)
			{
				auto &tag = morph->tags[i];
				ImGui::PushID(int(i));
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
				ImGui::InputText("##name", &tag.name);
				ImGui::SameLine();
				ui::attachButton(-1, tag.name);
				ImGui::SameLine();
				if (ImGui::SmallButton(ICON_FA_TRASH " Remove"))
				{
					morph->tags.erase(morph->tags.begin() + long(i));
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
		}
	}

	void ModelPropertyPanel::drawBonesMenu()
	{
		auto *model = dynamic_cast<SkeletalModel *>(singleton<State>().model.get());

		ImGui::Text("Bones: %zu", model->joints.size());
		boneCombo("Torso parent", model->joints, model->torsoParent, true);

		if (ImGui::Button(ICON_FA_CHECK " Bake tags"))
		{
			int count = bakeTags(*model);
			spdlog::get(MT_LOGGER)->info("Baked {} bones into tags", count);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Turn the bones used as tags into tags on their parent bones");
		}

		ImGui::InputText("Filter", &boneFilter);

		for (size_t i = 0; i < model->joints.size(); i++)
		{
			auto &joint = model->joints[i];
			if (!boneFilter.empty() && joint.name.find(boneFilter) == std::string::npos)
			{
				continue;
			}

			ImGui::PushID(int(i));
			if (ImGui::TreeNode("bone", "%zu: %s%s", i, joint.name.c_str(), joint.asTag ? " (tag)" : ""))
			{
				ImGui::InputText("Name", &joint.name);
				ImGui::Text("Parent: %s", joint.parentId >= 0 ? model->joints[joint.parentId].name.c_str() : "(none)");

				if (joint.asTag || canBeTag(*model, int(i)))
				{
					ImGui::Checkbox("Use as tag", &joint.asTag);
				}
				else
				{
					ImGui::TextDisabled("Can not be a tag (has children, weights, tags or hitboxes)");
				}

				ImGui::SliderFloat("Torso weight", &joint.torsoWeight, 0.0f, 1.0f);
				ui::attachButton(-1, joint.name);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void ModelPropertyPanel::drawSequencesMenu()
	{
		auto &state = singleton<State>();
		Model &model = *state.model;

		ImGui::Text("Sequences: %zu", model.animations.size());
		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_PLUS " Add"))
		{
			Animation animation;
			animation.name = "new_animation";
			animation.startFrame = std::max(model.currentFrame, 0);
			animation.numFrames = 1;
			animation.fps = 20;
			model.animations.push_back(animation);
		}

		ImGui::InputText("Filter", &sequenceFilter);

		for (size_t i = 0; i < model.animations.size(); i++)
		{
			auto &animation = model.animations[i];
			if (!sequenceFilter.empty() && animation.name.find(sequenceFilter) == std::string::npos)
			{
				continue;
			}

			ImGui::PushID(int(i));
			if (ImGui::SmallButton(ICON_FA_PLAY " Play"))
			{
				state.playback.setAnimation(animation);
				model.currentFrame = state.playback.frame();
				state.playback.play();
			}
			ImGui::SameLine();

			if (ImGui::TreeNode("sequence", "%s  %d-%d", animation.name.c_str(), animation.startFrame, animation.lastFrame()))
			{
				ImGui::InputText("Name", &animation.name);
				ImGui::InputInt("First frame", &animation.startFrame);
				ImGui::InputInt("Frames", &animation.numFrames);
				ImGui::InputInt("FPS", &animation.fps);
				ImGui::Checkbox("Looping", &animation.loop);
				ImGui::SameLine();
				ImGui::Checkbox("Reversed", &animation.reversed);
				ImGui::InputInt("Move speed", &animation.moveSpeed);
				ImGui::InputInt("Transition (ms)", &animation.transition);

				animation.startFrame = std::clamp(animation.startFrame, 0, std::max(model.numFrames() - 1, 0));
				animation.numFrames = std::max(animation.numFrames, 1);
				animation.fps = std::max(animation.fps, 1);

				if (ImGui::Button(ICON_FA_TRASH " Delete"))
				{
					model.animations.erase(model.animations.begin() + long(i));
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
}
