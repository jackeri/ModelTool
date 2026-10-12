#include "camera.h"

mt::Camera::Camera()
{
	modelViewCamera();
}

void mt::Camera::modelViewCamera()
{
	target = {0, 0, 0};
	angles = {-57.5f, -38.5f, 0};
	distance = 90.0f;
}

void mt::Camera::frame(const Bounds &bounds)
{
	glm::vec3 size = bounds.max - bounds.min;
	float radius = std::max(glm::length(size) * 0.5f, 1.0f);
	target = (bounds.min + bounds.max) * 0.5f;
	// The bounding sphere fits the view when its angular radius is half of the field of view
	distance = radius / std::sin(glm::radians(fov) * 0.5f) * 1.05f;
}

void mt::Camera::zoom(float steps)
{
	distance *= std::pow(1.0f - CAMERA_ZOOM_STEP, steps);
	distance = std::clamp(distance, 0.1f, 100000.0f);
}

void mt::Camera::rotate(float dx, float dy)
{
	angles.x = std::fmod(angles.x + dy * CAMERA_ROTATION_SCALE, 360.0f);
	angles.y = std::fmod(angles.y + dx * CAMERA_ROTATION_SCALE, 360.0f);
}

void mt::Camera::pan(float dx, float dy, float viewHeight)
{
	// World units per pixel at the target distance
	float scale = 2.0f * distance * std::tan(glm::radians(fov) * 0.5f) / std::max(viewHeight, 1.0f);
	glm::mat3 rotation = glm::mat3(glm::orientate4(glm::radians(angles)));
	// Rows of the rotation are the view axes in world space
	glm::vec3 right = glm::transpose(rotation)[0];
	glm::vec3 up = glm::transpose(rotation)[1];
	target -= right * dx * scale;
	target += up * dy * scale;
}

glm::mat4 mt::Camera::view() const
{
	return glm::translate(glm::vec3{0, 0, -distance}) * glm::orientate4(glm::radians(angles)) * glm::translate(-target);
}

glm::mat4 mt::Camera::projection(float aspect) const
{
	float nearPlane = std::max(distance * 0.01f, 0.01f);
	float farPlane = distance * 20.0f + 1000.0f;
	return glm::perspective(glm::radians(fov), std::max(aspect, 0.01f), nearPlane, farPlane);
}
