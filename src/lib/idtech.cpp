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

	glm::vec3 axisToAngles(const glm::mat3 &axis)
	{
		const glm::vec3 &forward = axis[0];
		const glm::vec3 &left = axis[1];
		const glm::vec3 &up = axis[2];

		const float pitch = std::asin(std::clamp(-forward.z, -1.0f, 1.0f));
		const float cp = std::cos(pitch);

		float yaw, roll;
		if (std::abs(cp) > 1e-5f)
		{
			yaw = std::atan2(forward.y, forward.x);
			roll = std::atan2(left.z, up.z);
		}
		else
		{
			// Looking straight up or down: roll and yaw are the same rotation, put it all in the yaw
			yaw = std::atan2(-left.x, left.y);
			roll = 0;
		}

		glm::vec3 angles;
		angles[PITCH] = glm::degrees(pitch);
		angles[YAW] = glm::degrees(yaw);
		angles[ROLL] = glm::degrees(roll);
		return angles;
	}

	glm::ivec2 vectorToIngles(const glm::vec3 &direction)
	{
		const float length = glm::length(direction);
		if (length <= 0.0f)
		{
			return {0, 0};
		}

		const glm::vec3 dir = direction / length;
		const float pitch = glm::degrees(std::asin(std::clamp(-dir.z, -1.0f, 1.0f)));
		const float yaw = glm::degrees(std::atan2(dir.y, dir.x));
		return {angleToShort(pitch), angleToShort(yaw)};
	}

	int16_t normalToLatLng(const glm::vec3 &normal)
	{
		// Straight up or down
		if (normal.x == 0.0f && normal.y == 0.0f)
		{
			return int16_t(normal.z > 0.0f ? 0 : 128);
		}

		const float scale = 128.0f / glm::pi<float>();
		int lat = int(std::lround(std::atan2(normal.y, normal.x) * scale)) & 0xff;
		int lng = int(std::lround(std::acos(std::clamp(normal.z, -1.0f, 1.0f)) * scale)) & 0xff;
		return int16_t((lat << 8) | lng);
	}

	glm::mat3 boneAxis(const glm::quat &rotation)
	{
		return glm::transpose(glm::mat3_cast(rotation));
	}
}
