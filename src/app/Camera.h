#pragma once

namespace mt {
	const float CAMERA_ZOOM_STEP = 0.25f;
	const float CAMERA_ROTATION_SCALE = 0.5f;

	class Camera {
	public:
		Camera();

		// Grid level camera
		void levelCamera();

		void modelViewCamera();

		void checkLimits(bool zoom = false);

		float xPos{}, yPos{}, zPos{};
		float rotAngleX{}, rotAngleY{}, rotAngleZ{};
		float scaleFactor{};
	};
}
