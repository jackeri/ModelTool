#include "ui_widgets.h"
#include "icons_font_awesome.h"
#include "state.h"

using namespace mt::model::skeletal;

namespace mt::ui {

	bool editRotation(const char *label, glm::quat &rotation)
	{
		glm::vec3 angles = glm::degrees(glm::eulerAngles(rotation));
		if (ImGui::DragFloat3(label, glm::value_ptr(angles), 0.5f, -180.0f, 180.0f, "%.1f"))
		{
			rotation = glm::normalize(glm::quat(glm::radians(angles)));
			return true;
		}
		return false;
	}

	bool boneCombo(const char *label, const std::vector<Joint> &joints, int &index, bool allowNone)
	{
		const char *preview = index >= 0 && index < int(joints.size()) ? joints[index].name.c_str() : "(none)";
		bool changed = false;
		if (ImGui::BeginCombo(label, preview))
		{
			if (allowNone && ImGui::Selectable("(none)", index < 0))
			{
				index = -1;
				changed = true;
			}
			for (int i = 0; i < int(joints.size()); i++)
			{
				ImGui::PushID(i);
				if (ImGui::Selectable(joints[i].name.c_str(), index == i))
				{
					index = i;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void attachButton(int parent, const std::string &point)
	{
		ImGui::PushID("attach");
		ImGui::PushID(point.c_str());
		if (ImGui::SmallButton(ICON_FA_LINK " Attach"))
		{
			ImGui::OpenPopup("attach");
		}
		ImGui::SetItemTooltip("Attach a model to %s", point.c_str());

		if (ImGui::BeginPopup("attach"))
		{
			ImGui::TextDisabled("Attach a model to %s", point.c_str());
			auto &state = singleton<State>();
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " From disk..."))
			{
				state.attachRequest = AttachRequest{parent, point, false};
			}
			if (ImGui::MenuItem(ICON_FA_ARCHIVE " From base folders..."))
			{
				state.attachRequest = AttachRequest{parent, point, true};
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		ImGui::PopID();
	}

}
