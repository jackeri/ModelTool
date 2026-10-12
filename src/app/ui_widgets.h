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

	/**
	 * Small "Attach" button with a menu (from the disk or the base folders) that asks for a model to attach to a tag or
	 * a bone (State::attachRequest)
	 * @param parent -1 for the main model, else the attachment index
	 * @param point tag or bone name
	 */
	void attachButton(int parent, const std::string &point);
}
