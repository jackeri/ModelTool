#include "idtech.h"

namespace mt::idtech {

	glm::mat3 anglesToAxis(const glm::vec3 &angles)
	{
		const float yaw = glm::radians(angles[YAW]);
		const float pitch = glm::radians(angles[PITCH]);
		const float roll = glm::radians(angles[ROLL]);

		const float sy = std::sin(yaw), cy = std::cos(yaw);
		const float sp = std::sin(pitch), cp = std::cos(pitch);
		const float sr = std::sin(roll), cr = std::cos(roll);

		glm::vec3 forward{cp * cy, cp * sy, -sp};
		glm::vec3 right{(-sr * sp * cy + cr * sy), (-sr * sp * sy - cr * cy), -sr * cp};
		glm::vec3 up{(cr * sp * cy + sr * sy), (cr * sp * sy - sr * cy), cr * cp};

		// idTech axis is forward, left, up
		return {forward, -right, up};
	}

	glm::vec3 ingleVector(int pitch, int yaw)
	{
		const float p = glm::radians(shortToAngle(pitch & 65535));
		const float y = glm::radians(shortToAngle(yaw & 65535));

		return {std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p)};
	}

	glm::vec3 latLngToNormal(int16_t normal)
	{
		const float lat = float((normal >> 8) & 0xff) * glm::pi<float>() / 128.0f;
		const float lng = float(normal & 0xff) * glm::pi<float>() / 128.0f;

		return {std::cos(lat) * std::sin(lng), std::sin(lat) * std::sin(lng), std::cos(lng)};
	}

	glm::quat boneRotation(const glm::mat3 &axis)
	{
		return glm::normalize(glm::quat_cast(glm::transpose(axis)));
	}
}
