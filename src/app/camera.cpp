#include "camera.h"

mt::Camera::Camera()
{
	modelViewCamera();
}

void mt::Camera::levelCamera()
{
	xPos = yPos = 0.0f;
	zPos = -2.f;
	rotAngleX = rotAngleY = 0;
	rotAngleZ = -90.0f;
	scaleFactor = 0.05f;
}

void mt::Camera::modelViewCamera()
{
	xPos = yPos = 0.f;
	zPos = -4.5f;
	rotAngleX = 32.5f;
	rotAngleY = -38.5f;
	rotAngleZ = -90.0f;
	scaleFactor = 0.05f;
}

void mt::Camera::checkLimits(bool zoom)
{
	if (!zoom)
	{
		// rotation limits
		if (rotAngleY > 360.0f)
		{
			rotAngleY = rotAngleY - 360.0f;
		}
		if (rotAngleY < -360.0f)
		{
			rotAngleY = rotAngleY + 360.0f;
		}
		if (rotAngleX > 360.0f)
		{
			rotAngleX = rotAngleX - 360.0f;
		}
		if (rotAngleX < -360.0f)
		{
			rotAngleX = rotAngleX + 360.0f;
		}
	}
	else
	{
		//zoom limits
		if (zPos < -1000.f)
		{
			zPos = -1000.f;
		}
		if (zPos > 1000.f)
		{
			zPos = 1000.f;
		}
	}
}
