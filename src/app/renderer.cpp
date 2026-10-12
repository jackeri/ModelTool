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

	void Renderer::grid(float size)
	{
		// About 30 - 60 lines over the area, on a power of two step like the game's grid
		const float GRID_STEP = std::exp2(std::round(std::log2(std::max(size, 1.0f) / 40.0f)));
		const float ORIGIN_LINE_LEN = std::ceil(size * 0.5f / GRID_STEP) * GRID_STEP;

		std::vector<LineVertex> lines;
		const glm::vec4 gridColor{0, 0, 0, 0.35f};
		for (float i = -ORIGIN_LINE_LEN; i <= ORIGIN_LINE_LEN; i += GRID_STEP)
		{
			lines.push_back({{i, -ORIGIN_LINE_LEN, 0}, gridColor});
			lines.push_back({{i, ORIGIN_LINE_LEN, 0}, gridColor});
			lines.push_back({{-ORIGIN_LINE_LEN, i, 0}, gridColor});
			lines.push_back({{ORIGIN_LINE_LEN, i, 0}, gridColor});
		}
		drawLines(lines, true);

		const glm::vec4 x{1, 0, 0, 1}, y{0, 1, 0, 1}, z{0, 0, 1, 1};
		drawLines({{{ORIGIN_LINE_LEN, 0, 0}, x},
				   {{-ORIGIN_LINE_LEN, 0, 0}, x},
				   {{0, ORIGIN_LINE_LEN, 0}, y},
				   {{0, -ORIGIN_LINE_LEN, 0}, y},
				   {{0, 0, ORIGIN_LINE_LEN}, z},
				   {{0, 0, -ORIGIN_LINE_LEN}, z}},
				  true);
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
