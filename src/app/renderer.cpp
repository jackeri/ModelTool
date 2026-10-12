#include "renderer.h"

namespace mt {

	void Renderer::push(const glm::mat4 &matrix)
	{
		stack.push_back(stack.back() * matrix);
	}

	void Renderer::pop()
	{
		if (stack.size() > 1)
		{
			stack.pop_back();
		}
	}

	glm::vec4 Renderer::gridColor(const glm::vec4 &background)
	{
		const float luminance = glm::dot(glm::vec3(background), glm::vec3(0.2126f, 0.7152f, 0.0722f));
		return luminance < 0.35f ? glm::vec4{1, 1, 1, 0.12f} : glm::vec4{0, 0, 0, 0.35f};
	}

	void Renderer::bounds(const Bounds &box, const glm::vec4 &color)
	{
		const glm::vec3 &a = box.min, &b = box.max;
		const glm::vec3 corners[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
									  {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
		const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

		std::vector<LineVertex> lines;
		for (const auto &edge : edges)
		{
			lines.push_back({corners[edge[0]], color});
			lines.push_back({corners[edge[1]], color});
		}
		drawLines(lines, false);
	}

	void Renderer::axis(float size)
	{
		const glm::vec4 x{1, 0, 0, 1}, y{0, 1, 0, 1}, z{0, 0, 1, 1};
		drawLines({{{0, 0, 0}, x}, {{size, 0, 0}, x}, {{0, 0, 0}, y}, {{0, size, 0}, y}, {{0, 0, 0}, z}, {{0, 0, size}, z}}, false);
		drawPoints({{{0, 0, 0}, {1, 0, 1, 1}}}, 7.0f, false);
	}
}
