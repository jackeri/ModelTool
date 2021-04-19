#pragma once

namespace mt {
	class UISystem {
	public:
		static bool setup(const char *);
		static void draw();
		static void shutdown();

	private:
		UISystem() = default;
	};
}
