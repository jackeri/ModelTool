#pragma once

#include "mt.h"

namespace mt {
	class State;
	class Camera;

	/**
	 * Apply the stored settings to the view, the playback and the camera
	 * @param state application state with the loaded settings
	 * @param camera scene camera
	 */
	void applySettings(State &state, Camera &camera);

	/**
	 * Store the current view, playback and camera settings into the settings
	 * @param state application state
	 * @param camera scene camera
	 */
	void collectSettings(State &state, const Camera &camera);
}
