#pragma once

namespace mt {
	class Camera;

	class Renderer {
	public:
		Renderer() = default;

		~Renderer() = default;

		Renderer(const Renderer &other) = delete;

		Renderer(Renderer &other) = delete;

		Renderer(Renderer &&other) = delete;

		virtual void setup()
		{}

		virtual void setView(int width, int height) = 0;

		virtual void startFrame(Camera &) = 0;

		virtual void grid() = 0;

		virtual void endFrame() = 0;

		Renderer &operator=(const Renderer &other) = delete;

		Renderer &operator=(Renderer &other) = delete;

		Renderer &operator=(Renderer &&other) = delete;
	};
}
