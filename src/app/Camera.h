#pragma once

namespace mt {
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
