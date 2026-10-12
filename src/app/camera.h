#pragma once

#include "mt.h"
#include "model.h"

namespace mt {
	const float CAMERA_ZOOM_STEP = 0.1f;	  ///< Zoom step, as a fraction of the distance
	const float CAMERA_ROTATION_SCALE = 0.5f; ///< Degrees per pixel when rotating
	const float DEFAULT_FOV = 90.0f;		  ///< Default vertical field of view in degrees

	/*! Orbit camera around a target point */
	class Camera {
	  public:
		Camera();

		/**
		 * Default view angles, looking at the origin
		 */
		void modelViewCamera();

		/**
		 * Point the camera at bounds so they fill the view
		 * @param bounds bounds to frame
		 */
		void frame(const Bounds &bounds);

		/**
		 * Zoom in (positive steps) or out (negative steps)
		 * @param steps scroll steps
		 */
		void zoom(float steps);

		/**
		 * Rotate around the target
		 * @param dx horizontal mouse movement in pixels
		 * @param dy vertical mouse movement in pixels
		 */
		void rotate(float dx, float dy);

		/**
		 * Move the target in the view plane
		 * @param dx horizontal mouse movement in pixels
		 * @param dy vertical mouse movement in pixels
		 * @param viewHeight height of the view in pixels
		 */
		void pan(float dx, float dy, float viewHeight);

		/**
		 * @return world to view matrix
		 */
		[[nodiscard]] glm::mat4 view() const;

		/**
		 * @return distance of the far clipping plane
		 */
		[[nodiscard]] float farPlane() const
		{
			return distance * 20.0f + 1000.0f;
		}

		/**
		 * @param aspect width / height of the view
		 * @return projection matrix
		 */
		[[nodiscard]] glm::mat4 projection(float aspect) const;

		glm::vec3 target{};		 ///< Point the camera orbits around
		glm::vec3 angles{};		 ///< Camera rotation angles (degrees)
		float distance = 90.0f;	 ///< Distance from the target
		float fov = DEFAULT_FOV; ///< Vertical field of view in degrees
	};
}
