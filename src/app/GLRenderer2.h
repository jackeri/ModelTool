#pragma once

#include "mt.h"
#include "renderer.h"

namespace mt {
	class GLRenderer2 : public Renderer {
	public:

		GLRenderer2();

		void setView(int width, int height) override;

		void push(const glm::mat4x4 &matrix) override;

		void pop() override;

		void startFrame(Camera &) override;

		void grid() override;

		void renderBuffer() override;

		void endFrame() override;
	};
}
