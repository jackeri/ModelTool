#include "app_icon.h"

#ifndef __APPLE__ // macOS is in app_icon_macos.mm

#include "embedded_files.h"
#include "mt.h"
#include "stb_image.h"

void mt::setAppIcon(GLFWwindow *window)
{
	const EmbeddedFile png = embeddedFile("icon.png");
	if (!png.data || !window)
	{
		return;
	}

	GLFWimage image{};
	int channels = 0;
	image.pixels = stbi_load_from_memory(png.data, int(png.size), &image.width, &image.height, &channels, 4);
	if (image.pixels)
	{
		// Wayland does not support window icons and reports an error, the desktop file gives the icon there
		if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
		{
			glfwSetWindowIcon(window, 1, &image);
		}
		stbi_image_free(image.pixels);
	}
}

#endif
