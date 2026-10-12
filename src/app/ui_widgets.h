#pragma once

#include "mt.h"
#include "skeletal_model.h"

namespace mt::ui {

	/**
	 * Edit a rotation as euler angles (degrees)
	 * @return true if the rotation changed
	 */
	bool editRotation(const char *label, glm::quat &rotation);

	/**
	 * Bone picker
	 * @param label combo label
	 * @param joints bones to pick from
	 * @param index selected bone, -1 for none
	 * @param allowNone add a "none" entry
	 * @return true if the selection changed
	 */
	bool boneCombo(const char *label, const std::vector<model::skeletal::Joint> &joints, int &index, bool allowNone);
}
