#pragma once

#include "library.h"
#include "model.h"

#include <array>

namespace mt::model {

	/*! How lengths are shown */
	enum class LengthUnit
	{
		Game,	  ///< game units
		Metric,	  ///< centimeters, meters from 1 m
		Imperial, ///< inches, feet and inches from 1 ft
	};

	/*! The usual Quake 3 / ET scale: one game unit is one inch */
	constexpr float METERS_PER_UNIT = 0.0254f;

	/**
	 * Show a length
	 * @param units length in game units
	 * @param unit how to show it
	 * @return text like "72.0 u", "1.83 m", "45.7 cm", "6 ft 0.0 in" or "8.5 in"
	 */
	std::string formatLength(float units, LengthUnit unit);

	/*! A dimension line of a measurement: the line, the extension lines from the measured box to it, and its length */
	struct DimensionLine {
		int axis = 0;											   ///< 0 = width (X), 1 = depth (Y), 2 = height (Z)
		glm::vec3 from{}, to{};									   ///< the dimension line, parallel to the axis
		std::vector<std::pair<glm::vec3, glm::vec3>> extensions{}; ///< from the box corners past the line ends
		float length = 0.0f;									   ///< length along the axis
		glm::vec3 label{};										   ///< middle of the line, where the value is shown
	};

	/**
	 * The width, depth and height lines of a box: the width along its front bottom edge (lowest Y), the depth along the
	 * bottom edge at the largest X, the height up the corner between them, each moved out of the box by the offset
	 * @param bounds measured box
	 * @param offset distance between the box and the lines
	 * @return width, depth and height lines
	 */
	std::array<DimensionLine, 3> dimensionLines(const Bounds &bounds, float offset);

	/**
	 * @param axis 0, 1 or 2
	 * @return "Width", "Depth" or "Height"
	 */
	const char *dimensionName(int axis);
}
