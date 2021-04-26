#pragma once

namespace mt {
	class Renderer {
	public:
		Renderer() = default;
		~Renderer() = default;

		virtual void startFrame() = 0;
		virtual void grid() = 0;
		virtual void endFrame() = 0;
	};
}
