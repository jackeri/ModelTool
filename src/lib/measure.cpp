#include "measure.h"

#include <cmath>
#include <cstdio>

namespace mt::model {

	namespace {
		std::string format(const char *pattern, double value)
		{
			char text[64];
			std::snprintf(text, sizeof(text), pattern, value);
			return text;
		}
	}

	std::string formatLength(float units, LengthUnit unit)
	{
		const double length = std::abs(double(units));
		switch (unit)
		{
		case LengthUnit::Metric: {
			const double meters = length * double(METERS_PER_UNIT);
			return meters < 1.0 ? format("%.1f cm", meters * 100.0) : format("%.2f m", meters);
		}
		case LengthUnit::Imperial: {
			// One unit is one inch; rounded to a tenth before splitting, so 11.96 in shows as 1 ft 0.0 in
			const double inches = std::round(length * 10.0) / 10.0;
			if (inches < 12.0)
			{
				return format("%.1f in", inches);
			}
			const double feet = std::floor(inches / 12.0);
			return format("%.0f ft ", feet) + format("%.1f in", inches - feet * 12.0);
		}
		case LengthUnit::Game:
		default: return format("%.1f u", length);
		}
	}

	std::array<DimensionLine, 3> dimensionLines(const Bounds &bounds, float offset)
	{
		const glm::vec3 &low = bounds.min;
		const glm::vec3 &high = bounds.max;
		const float reach = offset * 1.5f; // extension lines go a bit past the dimension line
		std::array<DimensionLine, 3> lines;

		// Width: along X in front of the box (lowest Y), on the floor of the box
		auto &width = lines[0];
		width.axis = 0;
		width.from = {low.x, low.y - offset, low.z};
		width.to = {high.x, low.y - offset, low.z};
		for (float x : {low.x, high.x})
		{
			width.extensions.push_back({{x, low.y, low.z}, {x, low.y - reach, low.z}});
		}

		// Depth: along Y on the right side of the box (largest X)
		auto &depth = lines[1];
		depth.axis = 1;
		depth.from = {high.x + offset, low.y, low.z};
		depth.to = {high.x + offset, high.y, low.z};
		for (float y : {low.y, high.y})
		{
			depth.extensions.push_back({{high.x, y, low.z}, {high.x + reach, y, low.z}});
		}

		// Height: up the front right corner, out of both sides
		auto &height = lines[2];
		height.axis = 2;
		height.from = {high.x + offset, low.y - offset, low.z};
		height.to = {high.x + offset, low.y - offset, high.z};
		for (float z : {low.z, high.z})
		{
			height.extensions.push_back({{high.x, low.y, z}, {high.x + reach, low.y - reach, z}});
		}

		for (auto &line : lines)
		{
			line.length = high[line.axis] - low[line.axis];
			line.label = (line.from + line.to) * 0.5f;
		}
		return lines;
	}

	const char *dimensionName(int axis)
	{
		static const char *names[3] = {"Width", "Depth", "Height"};
		return names[std::clamp(axis, 0, 2)];
	}
}
