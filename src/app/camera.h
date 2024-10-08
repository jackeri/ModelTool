#pragma once

#include "mt.h"

namespace mt {
	const float CAMERA_ZOOM_STEP = 0.25f;	  ///< Stepping used while zooming in or out
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

		glm::vec3 pos{};	 ///< Camera position
		glm::vec3 angles{};	 ///< Camera rotation angles
		float scaleFactor{}; ///< Camera scale

	  private:
		/**
		 * Clamp angle to be between -360 to 360 degrees
		 * @param angle angle to be clamped
		 */
		static void clampAngle(float &angle);

		/**
		 * Clamp position to be between -1000 and 1000 units
		 * @param pos position to be clamped
		 */
		static void clampPos(float &pos);
	};
}
