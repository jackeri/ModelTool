#include "camera.h"

mt::Camera::Camera()
{
	modelViewCamera();
}

void mt::Camera::levelCamera()
{
	pos.x = 0.f;
	pos.y = 0.f;
	pos.z = -2.f;
	angles.x = -90.0f;
	angles.y = 0;
	angles.z = 0;
	scaleFactor = 0.05f;
}

void mt::Camera::modelViewCamera()
{
	pos.x = 0.f;
	pos.y = 0.f;
	pos.z = -4.5f;
	angles.x = -57.5;
	angles.y = -38.5f;
	angles.z = 0;
	scaleFactor = 0.05f;
}

void mt::Camera::checkLimits(bool zoom)
{
	if (!zoom)
	{
		// rotation limits
		clampAngle(angles.x);
		clampAngle(angles.y);
		clampAngle(angles.z);
	}
	else
	{
		// zoom limits
		clampPos(pos.z);
	}
}

void mt::Camera::clampAngle(float &angle)
{
	if (angle > 360.0f)
	{
		angle = angle - 360.0f;
	}
	if (angle < -360.0f)
	{
		angle = angle + 360.0f;
	}
}

void mt::Camera::clampPos(float &pos)
{
	if (pos < -1000.f)
	{
		pos = -1000.f;
	}
	if (pos > 1000.f)
	{
		pos = 1000.f;
	}
}
