#include "ui_widgets.h"

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

}
