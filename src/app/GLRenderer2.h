#pragma once

#include "mt.h"
#include "renderer.h"

namespace mt {
	class GLRenderer2 : public Renderer {
	public:

		GLRenderer2();

		void startFrame() override;

		void grid() override;

		void endFrame() override;
	};
}
