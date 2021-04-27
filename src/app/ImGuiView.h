#pragma once

#include "mt.h"

namespace mt {
	class GLWindow;

	class ImGuiView {
	public:
		ImGuiView() = default;

		bool setup(GLWindow *window, const char *glslVersion);

		void startFrame();

		void endFrame();

		void draw();

		void shutdown();
	};
}
