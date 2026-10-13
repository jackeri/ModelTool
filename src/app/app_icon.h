#pragma once

struct GLFWwindow;

namespace mt {

	/**
	 * Shows the application icon while the program runs: the Dock icon on macOS (also when the executable is started
	 * directly, not through the bundle), the window icon on Windows and Linux (X11).
	 * The icon is the bundle's icon.icns on macOS, otherwise res/icon.png built into the program.
	 */
	void setAppIcon(GLFWwindow *window);
}
