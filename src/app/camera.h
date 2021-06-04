#pragma once

#include "mt.h"

namespace mt {
	const float CAMERA_ZOOM_STEP = 0.25f; ///< Stepping used while zooming in or out
	const float CAMERA_ROTATION_SCALE = 0.5f; ///< Stepping used while moving the camera (rotation)

	/*! Camera to handle the 3d viewport */
	class Camera {
	public:
		Camera();

		// Grid level camera
		void levelCamera();

		/**
		 * Set the camera to be a model view camera
		 */
		void modelViewCamera();

		/**
		 * Confirm that the calculated camera values are sane
		 * @param zoom check zooming limits or rotation limits
		 */
		void checkLimits(bool zoom = false);

		glm::vec3 pos{}; ///< Camera position
		glm::vec3 angles{}; ///< Camera rotation angles
		float scaleFactor{}; ///< Camera scale

	private:
		static void clampAngle(float &angle);
		static void clampPos(float &pos);
	};
}
