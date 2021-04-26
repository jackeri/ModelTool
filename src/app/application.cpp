#include <string>
#include "application.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "UISystem.h"
#include "filesystem.h"
#include "GLShader.h"
#include "GLRenderer2.h"

// FIXME: Remove when we have logger class..
#include <iostream>

namespace mt {

	static void glfwAppErrorCallback(int error, const char *description)
	{
		fprintf(stderr, "Glfw Error %d: %s\n", error, description);
	}

	Application::Application(const std::string &title)
	{
		this->title = title;
	}

	Application::~Application()
	{
		if (this->window)
		{
			// Handle UI system shutdown
			UISystem::shutdown();

			glfwDestroyWindow((GLFWwindow *) window);
			this->window = nullptr;
		}

		glfwTerminate();
	}

	bool Application::init()
	{
		// TODO: maybe move this all into a window class? Do we need multiple windows?
		glfwSetErrorCallback(glfwAppErrorCallback);
		if (!glfwInit())
		{
			return false;
		}

		glfwWindowHint(GLFW_SAMPLES, 4);

#if defined(MT_OPENGL_2)
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#elif defined(MT_OPENGL_3)
		// Let's use OpenGL 3.3 for now, maybe upgrade in the future
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
#error What GL version are we supposed to use?
#endif

		GLFWwindow *glfwWindow = glfwCreateWindow(1280, 720, this->title.c_str(), nullptr, nullptr);
		if (glfwWindow == nullptr)
		{
			return false;
		}
		this->window = glfwWindow;

		glfwMakeContextCurrent(glfwWindow);
		glfwSwapInterval(1);

		glewExperimental = GL_TRUE;
		if (glewInit() != GLEW_OK)
		{
			return false;
		}

		// Print the current GPU info
		const GLubyte *rendererStr = glGetString(GL_RENDERER);
		const GLubyte *version = glGetString(GL_VERSION);
		std::cout << "Renderer: " << rendererStr << std::endl;
		std::cout << "OpenGL version supported " << version << std::endl;

		// Init the UI system
		if (!UISystem::setup(glfwWindow, "#version 150")) {
			// UI setup failed, cancel the whole ordeal.
			return false;
		}

#if defined(MT_OPENGL_2)
		renderer = make_ref<GLRenderer2>();
#elif defined(MT_OPENGL_3)
#error Renderer implementation for Opengl3 missing
#endif

		return true;
	}

	void Application::loop()
	{
		auto *glfwWindow = static_cast<GLFWwindow *>(this->window);
		while (!glfwWindowShouldClose(glfwWindow))
		{
			glfwPollEvents();

			// Close the window on escape for now (faster dev)
			if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			{
				glfwSetWindowShouldClose(glfwWindow, true);
			}

			// Clear the screen for now
			glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			renderer->startFrame();

			renderer->grid();

			UISystem::draw();

			renderer->endFrame();

			glfwSwapBuffers(glfwWindow);
		}
	}
}