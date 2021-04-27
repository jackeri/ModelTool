#pragma once

#include "mt.h"

#include "ModelPropertyPanel.h"
#include "ScenePanel.h"
#include "GLWindow.h"

namespace mt {
	class UISystem {
	public:
		static bool setup(GLWindow *, const char *);
		static void draw();
		static void shutdown();

	private:
		UISystem() = default;
	};
}
