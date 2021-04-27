#include "GLWindow.h"
#include "GLRenderer2.h"
#include "UISystem.h"
#include <utility>

static void glfwAppErrorCallback(int error, const char *description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

mt::GLWindow::GLWindow(std::string title) : title(std::move(title))
{

}

mt::GLWindow::~GLWindow()
{
	destroy();
	glfwTerminate();
}

void mt::GLWindow::destroy()
{
	if (window)
	{
		UISystem::shutdown();
		glfwDestroyWindow(window);
		window = nullptr;
	}
}

bool mt::GLWindow::init()
{
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

	GLFWwindow *glfwWindow = glfwCreateWindow(1280, 720, title.c_str(), nullptr, nullptr);
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


#if defined(MT_OPENGL_2)
	renderer = make_ref<GLRenderer2>();
#elif defined(MT_OPENGL_3)
#error Renderer implementation for Opengl3 missing
#endif

	// Print the current GPU info
	const GLubyte *rendererStr = glGetString(GL_RENDERER);
	const GLubyte *version = glGetString(GL_VERSION);
	std::cout << "Renderer: " << rendererStr << std::endl;
	std::cout << "OpenGL version supported " << version << std::endl;

	// Init the UI system
	if (!UISystem::setup(this, "#version 150")) {
		// UI setup failed, cancel the whole ordeal.
		return false;
	}

	return true;
}

GLFWwindow *mt::GLWindow::system()
{
	return window;
}

void mt::GLWindow::loop()
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
